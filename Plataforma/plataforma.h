/*
 * Sistemas Operativos - Super Mario Proc RELOADED.
 * Grupo       : C o no ser.
 * Nombre      : plataforma.h.
 * Descripcion : Este archivo contiene los prototipos de las
 * funciones usadas por la plataforma.
 */

#ifndef PLATAFORMA_H_
#define PLATAFORMA_H_

#include <ginyu/protocolo.h>
#include <ginyu/config.h>
#include <ginyu/sockets.h>
#include <ginyu/list.h>
#include <commons/collections/queue.h>
#include <ginyu/log.h>

#include <sys/inotify.h>

#include <stdlib.h>
#include <curses.h>
#include <string.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <signal.h>
#include <pthread.h>
#include <errno.h>

#define PUERTO_PLAN 5050

typedef struct {
	int  socket;
	tSimbolo simbolo;
	t_list *recursos;
	int valorAlgoritmo; // Si está planificando con SRDF es la distancia al recurso y si planifica con RR es el quantum actual.
						//	Si el valor es negativo, es que el personaje no salio de la cola de listos
} tPersonaje;

typedef struct {
	int   socket; 			 // Socket de conexion con el nivel
	char* nombre; 			 // Nombre del nivel
	t_queue* cListos; 		 // Cola de listos
	t_list*  lBloqueados; 	 // Lista de bloqueados (ordenada por orden de llegada)
	t_list*  lMuertos;		 // Lista de personajes muertos
	fd_set masterfds;
	tAlgoritmo algoritmo;
	int quantum;
	int delay;
	bool hay_personajes;
	int maxSock;
} tNivel;

typedef struct {
	tPersonaje *pPersonaje;
	tSimbolo   recursoEsperado;
} tPersonajeBloqueado;

//Hilos
void *orquestador(void *) ;
void *planificador(void *);

bool sacarPersonajeDeListas(t_list *ready, t_list *block, int sock,  tPersonaje *pjLevantador);
void desbloquearPersonajes(t_list* block, t_list *ready, tPersonaje *pjLevantador, bool encontrado, char*nom_nivel, int proxPj);
void imprimirLista(tNivel *pNivel, tPersonaje *pPersonaje);
void marcarPersonajeComoReady(t_list *ready, int sock);


//Delegar conexiones
void delegarConexion(fd_set * conjuntoDestino, fd_set *conjuntoOrigen, int iSocket, int *maxSock);
void inicializarConexion(fd_set *master_planif, int *maxSock, int *sock);
void imprimirConexiones(fd_set *master_planif, int maxSock, char* host);
void signal_personajes(bool *hay_personajes);
void wait_personajes(bool *hay_personajes);

//Busquedas
int existeNivel(t_list * lNiveles, char* sLevelName);


//Constructores y destroyers
void agregarPersonaje(t_queue *cPersonajes, tSimbolo simbolo, int socket);
void crearNivel(t_list* lNiveles, tNivel* nivelNuevo, int socket, char *levelName, tInfoNivel *pInfoNivel);
void crearHiloPlanificador(pthread_t *pPlanificador, tNivel *nivelNuevo, t_list*);

#endif /* PLATAFORMA_H_ */
