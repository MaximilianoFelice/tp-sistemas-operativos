/*
 * Sistemas Operativos - Super Mario Proc RELOADED.
 * Grupo       : C o no ser.
 * Nombre      : sockets.c
 * Descripcion : Este archivo los implementacion de la libreria de sockets.
 */

#include "sockets.h"

#define BACKLOG 100

int crearSocket(t_log* logger)
{
	int unSocket;
	int si = 1;
	//--Crea el socket
	if ((unSocket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		log_error(logger, "Creacion socket: %s", strerror(errno));
		return EXIT_FAILURE;

	} else {
		//--Setea las opciones para que pueda escuchar varios al mismo tiempo
		if (setsockopt(unSocket, SOL_SOCKET, SO_REUSEADDR, &si, sizeof(int)) == -1) {
			log_error(logger, "Setsockopt: %s", strerror(errno));
			return EXIT_FAILURE;
		}

		return unSocket;
	}
}

void bindearSocket(int unSocket, struct sockaddr_in socketInfo, t_log* logger)
{
	//--Bindear socket al proceso server
	if (bind(unSocket, (struct sockaddr*)&socketInfo, sizeof(socketInfo)) == -1) {
		log_error(logger, "Error al bindear socket escucha: %s", strerror(errno));
		exit(EXIT_FAILURE);
	}
}

void escucharEn(int unSocket)
{
	if (listen(unSocket, BACKLOG) == -1) {
		perror("Error al poner a escuchar socket");
		exit(EXIT_FAILURE);
	}
}


int crearSocketEscucha(fd_set* master, struct sockaddr_in remoteAddress, int *maxSock, int puerto, t_log* logger)
{
	struct sockaddr_in myAddress;
	FD_ZERO(master);
	int socketEscucha;

	socketEscucha = crearSocket(logger);

	//--Arma la información que necesita para mandar cosas
	myAddress.sin_family = AF_INET;
	myAddress.sin_addr.s_addr = INADDR_ANY;
	myAddress.sin_port = htons(puerto);
	memset(&(myAddress.sin_zero), '\0', 8);  	 // Poner a cero el resto de la estructura

	bindearSocket(socketEscucha, myAddress,logger);

	//--Escuchar
	escucharEn(socketEscucha);

	//--Prepara la lista
	FD_SET(socketEscucha, master);
	*maxSock = socketEscucha;

	return socketEscucha;
}


int enviarPaquete(int socketServidor, tPaquete* pPaqueteAEnviar, t_log* logger, char* info)
{
	int byteEnviados;
	log_debug(logger, ">>> %s", info);

	byteEnviados = send(socketServidor, (char *)pPaqueteAEnviar, sizeof(tHeader) + pPaqueteAEnviar->length, 0);

	if (byteEnviados == -1) {
		log_error(logger, "%s: %s", info, strerror(errno));
		return -1;

	} else {
		return byteEnviados;
	}
}

int recibirPaquete(int socketReceptor, tMensaje* tipoMensaje, void** buffer, t_log* pLogger, char* sMensajeLogger)
{
	tHeader header;
	int bytesRecibidosHeader;
	int bytesRecibidos;

	log_debug(pLogger, "<<< %s", sMensajeLogger);
	bytesRecibidosHeader = recv(socketReceptor, &header, sizeof(tHeader), MSG_WAITALL);

	if (bytesRecibidosHeader == 0) {
		return 0;	// CERRO CONEXION

	} else if (bytesRecibidosHeader < 0) {
		log_error(pLogger, "%s: %s", sMensajeLogger,  strerror(errno));
		return -1;	// ERROR
	}

	*buffer = malloc(header.length);

	bytesRecibidos = recv(socketReceptor, *buffer, header.length, MSG_WAITALL);

	*tipoMensaje = (tMensaje) header.type;

	if (bytesRecibidos < 0) {
		log_error(pLogger, "%s: %s", sMensajeLogger,  strerror(errno));
		free(buffer);	// ERROR, se libera el espacio reservado
		return -1;
	}

	return bytesRecibidos;
}


/*
 * @NAME: getSockChanged
 * @DESC: Multiplexa con Select
 *
 * 	Valores de salida:
 * 	=0 = se agrego un nuevo soquet
 * 	<0 = Se cerro el soquet que devuelce
 * 	>0 = Cambio el soquet que devuelce
 */
signed int getConnection(fd_set *master, int *maxSock, int sockListener, struct sockaddr_in *remoteAddress, tMensaje *tipoMensaje, void **buffer, t_log* logger, char* emisor)
{
	int addressLength;
	int unSocket;
	int newSock;
	int nBytes;
	fd_set temp;
	FD_ZERO(&temp);
	temp = *master;

	//--Multiplexa conexiones
	if (select(*maxSock + 1, &temp, NULL, NULL, NULL ) == -1) {
		log_error(logger, "select: %s", strerror(errno));
		exit(EXIT_FAILURE);
	}

	//--Cicla las conexiones para ver cual cambió
	for (unSocket = 0; unSocket <= *maxSock; unSocket++) {

		//--Si el i° socket cambió
		if (FD_ISSET(unSocket, &temp)) {
			//--Si el que cambió, es el listener

			if (unSocket == sockListener) {
				addressLength = sizeof(*remoteAddress);
				//--Gestiona nueva conexión
				newSock = accept(sockListener, (struct sockaddr *) remoteAddress, (socklen_t *) &addressLength);
				log_trace(logger, "%s: Nueva conexion en %d", emisor, newSock);

				if (newSock == -1) {
					log_error(logger, "accept: %s", strerror(errno));

				} else {
					//--Agrega el nuevo listener
					FD_SET(newSock, master);

					if (newSock > *maxSock) {
						*maxSock = newSock;
					}
				}

			} else {
				//--Gestiona un cliente ya conectado

				if ((nBytes = recibirPaquete(unSocket, tipoMensaje, buffer, logger, "Se recibe informacion")) <= 0) {

					//--Si cerró la conexión o hubo error
					if (nBytes == 0) {
						log_trace(logger, "%s: Fin de conexion de %d.", emisor, unSocket);

					} else {
						log_error(logger, "recv: %s", strerror(errno));
					}

					//--Cierra la conexión y lo saca de la lista
					close(unSocket);
					FD_CLR(unSocket, master);

				} else {
					return unSocket;
				}
			}
		}
	}
	return -1;
}



signed int multiplexar(fd_set *master, fd_set *temp, int *maxSock, tMensaje* tipoMensaje, void **buffer, t_log* logger)
{
	int i;
	int nBytes;
	memcpy(temp, master, sizeof(fd_set));

	//--Multiplexa conexiones
	if (select(*maxSock + 1, temp, NULL, NULL, NULL) == -1) {
		log_error(logger, "select: %s", strerror(errno));
		exit(EXIT_FAILURE);
	}

	//--Cicla las conexiones para ver cual cambió
	for (i = 0; i <= *maxSock; i++) {

		if (FD_ISSET(i, temp)) {
			//--Gestiona un cliente ya conectado

			if ((nBytes = recibirPaquete(i, tipoMensaje, buffer, logger, "Se recibe Mensaje")) <= 0) {
				//--Si cerró la conexión o hubo error
				if (nBytes == 0) {
					log_trace(logger,"Planificador: Fin de conexion de %d.", i);
				} else {
					log_error(logger, "Planificador: recv: %s", strerror(errno));
				}
				//--Cierra la conexión y lo saca de la lista
				close(i);
				FD_CLR(i, master);

			} else { //Tengo info
				return i;
			}

		}
	}
	return -1;
}


void esperarMensaje(int sock, void *msj, int size, t_log* logger)
{
	if ((recv(sock, msj, size, 0)) == -1) {
		log_error(logger, "recv: %s", strerror(errno));
		exit(EXIT_FAILURE);
	}
}

void esperarTurno(int sock, void *msj, int size, t_log* logger)
{
	esperarMensaje(sock, msj, size, logger);
}

signed int connectServer(char *ip_server, int puerto, t_log *logger, char *host)
{
	int sockfd; 	// Escuchar sobre sock_fd, nuevas conexiones sobre new_fd
	struct sockaddr_in their_addr; 	// Información sobre mi dirección

	////SETEO CONFIGURACIONES DE IP + PUERTO
	their_addr.sin_family = AF_INET;  // Ordenación de bytes de la máquina
	their_addr.sin_port = htons(puerto); // short, Ordenación de bytes de la	red
	their_addr.sin_addr.s_addr = inet_addr(ip_server);
	memset(&(their_addr.sin_zero), '\0', 8); // Poner a cero el resto de la estructura

	////PIDO EL SOCKET
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		log_error(logger, "socket: %s", strerror(errno));
		exit(EXIT_FAILURE);
	}

	////INTENTO CONECTAR
	if (connect(sockfd, (struct sockaddr *) &their_addr, sizeof their_addr) == -1) {
		log_error(logger, "connect: %s", strerror(errno));
		exit(EXIT_FAILURE);
	}

	log_trace(logger, "Conectado con %s en socket %d", host, sockfd);

	return sockfd;
}
