/*
 * sockets.h
 *
 *  Created on: 08/05/2013
 *      Author: utnso
 */

#ifndef GINYUSOCKETS_H_
#define GINYUSOCKETS_H_

#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "../commons/log.h"
#include <errno.h>

void iniSocks(fd_set *master, fd_set *temp, struct sockaddr_in *myAddress,
		struct sockaddr_in remoteAddress, int *maxSock, int *sockListener,
		int puerto, t_log* logger);

int selectSocks(fd_set *master, fd_set *temp, int *maxSock, int sockListener,
		struct sockaddr_in remoteAddress, void *buf);

signed int getSockChanged(fd_set *master, fd_set *temp, int *maxSock,
		int sockListener, struct sockaddr_in *remoteAddress, void *buf,
		int bufSize, t_log* logger, char *);
signed int multiplexar(fd_set *master, fd_set *temp,int *maxSock, void *buffer, int bufferSize, t_log*) ;
signed int getSockChangedNB(fd_set *master, fd_set *temp, int *maxSock,
		int sockListener, struct sockaddr_in *remoteAddress, void *buf,
		int bufSize, t_log* logger, int secs);
void enviaMensaje(int numSock, void* message, int size, t_log* logger,
		char* info);
void mandarMensaje(int numSock, void* message, int size, t_log *logger);

void recibeMensaje(int numSock, void* message, int size, t_log* logger,
		char* info);

void esperarMensaje(int sock, void *msj, int size, t_log* logger);
void esperarTurno(int sock, void *msj, int size, t_log* logger);

signed int connectServer(char *ip_server, int puerto, t_log *logger, char *host);

#endif /* SOCKETS_H_ */
