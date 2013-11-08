#include "epoll.h"

static int make_socket_non_blocking(int);

struct epoll_event * iniciarConexiones(int *instancia_epoll,
		struct epoll_event *event, struct sockaddr_in *myAddress,
		int *sockListener, int puerto, t_log* logger) {

	struct epoll_event *events;

	////PIDO EL SOCKET
	if ((*sockListener = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		log_error(logger, "socket: %s", strerror(errno));
		exit(EXIT_FAILURE);
	}

	////SETEO CONFIGURACIONES DE IP + PUERTO
	myAddress->sin_family = AF_INET;  // Ordenación de bytes de la máquina
	myAddress->sin_port = htons(puerto); // short, Ordenación de bytes de la	red
	myAddress->sin_addr.s_addr = INADDR_ANY; // Rellenar con mi dirección IP
	memset(&(myAddress->sin_zero), '\0', 8); // Poner a cero el resto de la estructura

	int yes = 1;

	//--Setea las opciones para que pueda escuchar varios al mismo tiempo
	if (setsockopt(*sockListener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int))
			== -1) {
		log_error(logger, "setsockopt: %s", strerror(errno));
		exit(EXIT_FAILURE);
	}

	////ASIGNO AL SOCKET LAS CONFIGURACIONES DE IP + PUERTO
	if (bind(*sockListener, (struct sockaddr *) myAddress, sizeof(*myAddress))
			== -1) {
		log_error(logger, "bind: %s", strerror(errno));
		exit(EXIT_FAILURE);
	}

	////HAGO EL SOCKET NO BLOQUEANTE
	if ((make_socket_non_blocking(*sockListener)) == -1)
		abort();

	////LISTEN (BACKLOG => MAX CANTIDAD DE CLIENTES EN COLA)
	if (listen(*sockListener, BACKLOG) == -1) {
		log_error(logger, "listen: %s", strerror(errno));
		exit(EXIT_FAILURE);
	}

	////CREO LA INSTANCIA DE EPOLL
	if ((*instancia_epoll = epoll_create1(0)) == -1) {
		log_error(logger, "epoll_create: %s", strerror(errno));
		exit(EXIT_FAILURE);
	}

	////ASOCIO LOS FD DE LA CONEXION MAESTRA
	event->data.fd = *sockListener;
	event->events = EPOLLIN | EPOLLET | EPOLLRDHUP;

	////ASIGNO EL CCONTROL DE EPOLL
	if ((epoll_ctl(*instancia_epoll, EPOLL_CTL_ADD, *sockListener, event))
			== -1) {
		log_error(logger, "epoll_ctl: %s", strerror(errno));
		exit(EXIT_FAILURE);
	}

	////RESERVO ESPACIO PARA LAS CONEXIONES
	events = calloc(MAXEVENTS, sizeof event);

	return events;

}

signed int getSockEpoll(int *instancia_epoll, struct epoll_event *event,
		struct epoll_event *events, int sockListener,
		struct sockaddr_in *remoteAddress, void *buf, int bufSize,
		t_log* logger) {

	////ESPERO MENSAJES O CONEXIONES
	int n, i; // i = variable para recorrer los eventos

	//// ESPERO LAS NOVEDADES EN LOS SOCKETS QUE ESTOY OBSERVANDO
	n = epoll_wait(*instancia_epoll, events, MAXEVENTS, -1); // n = cantidad de eventos que devuelve epoll

	if (n == -1) {
		log_error(logger, "epoll_wait:%s", strerror(errno));
		exit(EXIT_FAILURE);
	}

	//// RECORRO LOS EVENTOS ATENDIENDO LAS NOVEDADES
	for (i = 0; i < n; i++) {
		//// SI EL EVENTO QUE ESTOY MIRANDO DIO ERROR O NO ESTA LISTO PARA SER LEIDO
		if ((events[i].events & EPOLLERR) || (events[i].events & EPOLLRDHUP) || (events[i].events & EPOLLHUP) || (!(events[i].events & EPOLLIN))) {
			log_trace(logger, "Fin de conexion en %d", events[i].data.fd);
			close(events[i].data.fd);
			//continue;
			break;
		} else {
			//// HAY NOVEDADES EN EL SOCKET MAESTRO (NUEVAS CONEXIONES)!!!
			/*else*/
			if (sockListener == events[i].data.fd) {

				////ACEPTO TODAS LAS INCOMING CONNECTIONS
				//while (1) {
					socklen_t in_len = sizeof(struct sockaddr);
					int newSock;
					char hbuf[30], sbuf[30];

					////ASIGNO EL NUEVO SOCKET DESCRIPTOR
					newSock = accept(sockListener,(struct sockaddr *) remoteAddress, &in_len);

					////SI YA HABIA ACEPTADO TODAS O AL ACEPTAR UNA CONEXION ME DA ERROR
					if (newSock == -1) {
						if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
							//YA ACEPTÉ TODAS LAS CONEXIONES NUEVAS, SALGO DEL BUCLE
							break;
						} else {
							//ERROR AL QUERER ACEPTAR, SALGO DEL BUCLE
							log_error(logger, "accept: %s", strerror(errno));
							break;
						}
					}

					////OBTENGO LOS DATOS DEL CLIENTE
					int s = getnameinfo((struct sockaddr *) remoteAddress, in_len,
							hbuf, sizeof hbuf, sbuf, sizeof sbuf, 0);
					if (s == 0)
						log_trace(logger,"Nueva conexion en socket: %d (host=%s, puerto=%s)",
								newSock, hbuf, sbuf);

					//SETEO EL SOCKET COMO NO BLOQUEANTE
					if (make_socket_non_blocking(newSock) == -1) {
						log_debug(logger, "make_socket_non_bloking");
						abort();
					}

					////ASOCIO EL FD DE LA NUEVA CONEXION
					event->data.fd = newSock;
					event->events = EPOLLIN | EPOLLET;

					//AGREGO LA NUEVA CONEXION A LA INSTANCIA EPOLL
					if (epoll_ctl(*instancia_epoll, EPOLL_CTL_ADD, newSock, event)
							== -1) {
						log_error(logger, "epoll_ctl: %s", strerror(errno));
						exit(EXIT_FAILURE);
					}
				//} //CIERRE DEL WHILE; NO ERAN CONEXIONES NUEVAS
				//continue;

			} else { ////HAY DATOS EN ALGUNA CONEXION, TENGO QUE LEER T0D0

				//while (1) {
				//LEO LOS DATOS
				ssize_t nBytes = read(events[i].data.fd, buf, bufSize);

				if (nBytes <=0) {//CHECKEO SI YA LEI TODOS O EL CLIENTE CERRO CONEXION
					if (nBytes == -1) {
						if (errno != EAGAIN) { //LEI TODOS
							log_error(logger, "read: %s", strerror(errno));
							close(events[i].data.fd);
						}
					} else if (nBytes == 0) { //EL CLIENTE CERRO CONEXION
						log_error(logger, "Fin de conexion en %d", events[i].data.fd);
						close(events[i].data.fd);
					}
					break;
				} else {
					return events[i].data.fd;
				}
				//}
			}
		}
	}
	return -1;
}

static int make_socket_non_blocking(int sfd) {
	int flags, s;

	flags = fcntl(sfd, F_GETFL, 0);
	if (flags == -1) {
		perror("fcntl");
		return -1;
	}

	flags |= O_NONBLOCK;
	s = fcntl(sfd, F_SETFL, flags);
	if (s == -1) {
		perror("fcntl");
		return -1;
	}
	return 0;
}

signed int connectServerEpoll(char *ip_server, int puerto, t_log *logger) {
	int sockfd; 	// Escuchar sobre sock_fd, nuevas conexiones sobre new_fd
	struct sockaddr_in their_addr; 	// Información sobre mi dirección
	struct hostent *server; 		// Información sobre el server

	////OBTENER INFORMACIÓN DEL SERVER
	if ((server = gethostbyname(ip_server)) == NULL ) { // obtener información de máquina
		log_error(logger, "gethostbyname: %s", strerror(errno));
		exit(EXIT_FAILURE);
	}

	////SETEO CONFIGURACIONES DE IP + PUERTO
	their_addr.sin_family = AF_INET;  // Ordenación de bytes de la máquina
	their_addr.sin_port = htons(puerto); // short, Ordenación de bytes de la	red
	their_addr.sin_addr = *((struct in_addr *) server->h_addr);
	memset(&(their_addr.sin_zero), '\0', 8); // Poner a cero el resto de la estructura

	////PIDO EL SOCKET
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		log_error(logger, "socket: %s", strerror(errno));
		exit(EXIT_FAILURE);
	}

	////INTENTO CONECTAR
	if (connect(sockfd, (struct sockaddr *) &their_addr,
			sizeof(struct sockaddr)) == -1) {
		log_error(logger, "connect: %s", strerror(errno));
		exit(EXIT_FAILURE);
	}

	log_debug(logger, "Conectado con host");

	return sockfd;
}

void recvMsjEpoll(int numSock, void* message, int size, t_log* logger, char* info){
	log_debug(logger, "<<< %s", info);
	if(read(numSock, message, size) == -1){
		log_error(logger, "%s: %s", info,  strerror(errno));
		exit(EXIT_FAILURE);
	}
}
