/*
 * Sistemas Operativos - Super Mario Proc RELOADED.
 * Grupo       : C o no ser.
 * Nombre      : sockets.h.
 * Descripcion : Este archivo los prototipos de la libreria de sockets.
 */

#ifndef LIBSOCKETS_H_
#define LIBSOCKETS_H_

#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <commons/log.h>
#include <errno.h>
#include <stdlib.h>
#include <curses.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <signal.h>

#include "protocolo.h"

#define MAX_BUFFER 1024

typedef struct {
	int8_t  type;
	int16_t length;
} tHeader;

typedef struct {
	int8_t  type;
	int16_t length;
	char    payload[MAX_BUFFER];
} tPaquete;

void iniSocks(fd_set *master, struct sockaddr_in *myAddress, struct sockaddr_in remoteAddress, int *maxSock, int *sockListener, int puerto, t_log* logger);

int enviarPaquete(int socketServidor, tPaquete* buffer, t_log* logger, char* info);

int recibirPaquete(int socketReceptor, tMensaje* tipoMensaje, void** buffer, t_log* pLogger, char* sMensajeLogger);

signed int getConnection(fd_set *master, int *maxSock, int sockListener, struct sockaddr_in *remoteAddress, tMensaje *tipoMensaje, void **buffer, t_log* logger, char* emisor);

signed int multiplexar(fd_set *master, fd_set *temp, int *maxSock, tMensaje* tipoMensaje, void **buffer, t_log* logger);

void esperarMensaje(int sock, void *msj, int size, t_log* logger);

void esperarTurno(int sock, void *msj, int size, t_log* logger);

signed int connectServer(char *ip_server, int puerto, t_log *logger, char *host);

#endif /* LIBSOCKETS_H_ */
