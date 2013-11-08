#ifndef EPOLL_H_
#define EPOLL_H_

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <string.h>
#include <signal.h>
#include <sys/epoll.h>
#include <netdb.h>
#include <fcntl.h>
#include <commons/log.h>
#include <commons/collections/list.h>

#define MYPORT 3490
#define BACKLOG 20
#define MAXEVENTS 20

typedef struct _Connection {
	char client[20];
	int client_FD;
	struct sockaddr client_addr;
	socklen_t in_len;
} Connection;

typedef struct _Mensaje {
	char from[20];
	char data[128];
} Mensaje;

////PROTOTIPOS DE FUNCIONES
struct epoll_event * iniciarConexiones(int *instancia_epoll,
		struct epoll_event *event, struct sockaddr_in *myAddress,
		int *sockListener, int puerto, t_log* logger);
signed int getSockEpoll(int *instancia_epoll, struct epoll_event *event,
		struct epoll_event *events, int sockListener,
		struct sockaddr_in *remoteAddress, void *buf, int bufSize,
		t_log* logger);
signed int connectServerEpoll(char *ip_server, int puerto, t_log *logger);
void recvMsjEpoll(int numSock, void* message, int size, t_log* logger, char* info);

#endif /* EPOLL_H_ */
