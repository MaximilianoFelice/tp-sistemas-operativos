#include <stdlib.h>
#include <curses.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <signal.h>
#include "../commons/log.h"
#include "protocolo.h"
#include "sockets.h"




void iniSocks(fd_set *master, fd_set *temp, struct sockaddr_in *myAddress, struct sockaddr_in remoteAddress, int *maxSock, int *sockListener, int puerto, t_log* logger) {

	int yes = 1;

	FD_ZERO(master);
	FD_ZERO(temp);

	//--Crea el socket
	if ((*sockListener = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		log_error(logger, "socket: %s", strerror(errno));
		exit(1);
	}

	//--Setea las opciones para que pueda escuchar varios al mismo tiempo
	if (setsockopt(*sockListener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
		log_error(logger, "setsockopt: %s", strerror(errno));
		exit(1);
	}

	//--Arma la información que necesita para mandar cosas
	myAddress->sin_family = AF_INET;
	myAddress->sin_addr.s_addr = INADDR_ANY;
	myAddress->sin_port = htons(puerto);

	//--Bindear socket al proceso server
	if (bind(*sockListener, (struct sockaddr *) myAddress, sizeof(*myAddress)) == -1) {
		log_error(logger, "bind: %s", strerror(errno));
		exit(1);
	}

	//--Escuchar
	if (listen(*sockListener, 100) == -1) {
		log_error(logger, "listen: %s", strerror(errno));
		exit(1);
	}

	//--Prepara la lista
	FD_SET(*sockListener, master);
	*maxSock = *sockListener;
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
signed int getSockChanged(fd_set *master, fd_set *temp, int *maxSock, int sockListener, struct sockaddr_in *remoteAddress, void *buf, int bufSize, t_log* logger, char* emisor) {

	int addressLength;
	int i;
	int newSock;
	int nBytes;

	*temp = *master;

	//--Multiplexa conexiones
	if (select(*maxSock + 1, temp, NULL, NULL, NULL ) == -1) {
		log_error(logger, "select: %s", strerror(errno));
		exit(1);
	}

	//--Cicla las conexiones para ver cual cambió
	for (i = 0; i <= *maxSock; i++) {
		//--Si el i° socket cambió
		if (FD_ISSET(i, temp)) {
			//--Si el que cambió, es el listener
			if (i == sockListener) {
				addressLength = sizeof(*remoteAddress);
				//--Gestiona nueva conexión
				newSock = accept(sockListener, (struct sockaddr *) remoteAddress, (socklen_t *) &addressLength);
				log_trace(logger, "%s: Nueva conexion en %d", emisor, newSock);
				if (newSock == -1)
					log_error(logger, "accept: %s", strerror(errno));
				else {
					//--Agrega el nuevo listener
					FD_SET(newSock, master);
					if (newSock > *maxSock)
						*maxSock = newSock;
				}
			} else {
				//--Gestiona un cliente ya conectado
				if ((nBytes = recv(i, buf, bufSize, 0)) <= 0) {

					//--Si cerró la conexión o hubo error
					if (nBytes == 0)
						log_trace(logger, "%s: Fin de conexion de %d.", emisor, i);
					else
						log_error(logger, "recv: %s", strerror(errno));

					//--Cierra la conexión y lo saca de la lista
					close(i);
					FD_CLR(i, master);
				} else {
					return i;
				}
			}
		}
	}
	return -1;
}

signed int getSockChangedNB(fd_set *master, fd_set *temp, int *maxSock, int sockListener, struct sockaddr_in *remoteAddress, void *buf, int bufSize, t_log* logger, int secs) {

	int addressLength;
	int i;
	int newSock;
	int nBytes;
	struct timeval timeout;
	int res;

	timeout.tv_sec = secs;
	timeout.tv_usec = 0;

	*temp = *master;

	//--Multiplexa conexiones
	if ((res = select(*maxSock + 1, temp, NULL, NULL, &timeout)) == -1) {
		log_error(logger, "select: %s", strerror(errno));
		exit(1);
	}

	if (res == 0) {//--Si sale por timeout
		return 0;
	} else {
		//--Cicla las conexiones para ver cual cambió
		for (i = 0; i <= *maxSock; i++) {
			//--Si el i° socket cambió
			if (FD_ISSET(i, temp)) {
				//--Si el que cambió, es el listener
				if (i == sockListener) {
					addressLength = sizeof(*remoteAddress);
					//--Gestiona nueva conexión
					newSock = accept(sockListener, (struct sockaddr *) remoteAddress, (socklen_t *) &addressLength);
					log_trace(logger, "Nueva coneccion en %d", newSock);
					if (newSock == -1)
						log_error(logger, "accept: %s", strerror(errno));
					else {
						//--Agrega el nuevo listener
						FD_SET(newSock, master);
						if (newSock > *maxSock)
							*maxSock = newSock;
					}
				} else {
					//--Gestiona un cliente ya conectado
					if ((nBytes = recv(i, buf, bufSize, 0)) <= 0) {

						//--Si cerró la conexión o hubo error
						if (nBytes == 0)
							log_trace(logger, "Fin de conexion de %d.", i);
						else
							log_error(logger, "recv: %s", strerror(errno));

						//--Cierra la conexión y lo saca de la lista
						close(i);
						FD_CLR(i, master);
					} else {
						return i;
					}
				}
			}
		}
	}
	return -1;
}

void enviaMensaje(int numSock, void* message, int size, t_log* logger, char* info) {
	log_debug(logger, ">>> %s", info);
	if (send(numSock, message, size, 0) == -1) {
		log_error(logger, "%s: %s", info, strerror(errno));
		exit(EXIT_FAILURE);
	}
}

void mandarMensaje(int numSock, void* message, int size, t_log *logger){
	if (send(numSock, message, size, 0) == -1) {
		log_error(logger, "send: %s", strerror(errno));
		exit(EXIT_FAILURE);
	}
}

void recibeMensaje(int numSock, void* message, int size, t_log* logger, char* info) {
	log_debug(logger, "<<< %s", info);
	if ((recv(numSock, message, size, 0)) == -1) {
		log_error(logger, "%s: %s", info,  strerror(errno));
		exit(EXIT_FAILURE);
	}
}

void esperarMensaje(int sock, void *msj, int size, t_log* logger){
	if ((recv(sock, msj, size, 0)) == -1) {
			log_error(logger, "recv: %s", strerror(errno));
			exit(EXIT_FAILURE);
	}
}

void esperarTurno(int sock, void *msj, int size, t_log* logger){
	esperarMensaje(sock, msj, size, logger);
}

signed int connectServer(char *ip_server, int puerto, t_log *logger, char *host) {

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
