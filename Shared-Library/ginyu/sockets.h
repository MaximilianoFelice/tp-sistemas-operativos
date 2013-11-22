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


int crearSocketEscucha(int puerto, t_log* logger);

int enviarPaquete(int socketServidor, tPaquete* buffer, t_log* logger, char* info);

int recibirPaquete(int socketReceptor, tMensaje* tipoMensaje, char** payload, t_log* pLogger, char* sMensajeLogger);

signed int getConnection(fd_set *master, int *maxSock, int sockListener, tMensaje *tipoMensaje, char** payload, t_log* logger);

signed int multiplexar(fd_set *master, fd_set *temp, int *maxSock, tMensaje* tipoMensaje, char** buffer, t_log* logger);

signed int connectToServer(char *ip_server, int puerto, t_log *logger);

#endif /* LIBSOCKETS_H_ */
