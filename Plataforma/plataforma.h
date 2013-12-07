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
#include <ginyu/log.h>
#include <commons/collections/queue.h>

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

typedef struct {
	int  socket;
	tSimbolo simbolo;
	t_list *recursos;
	int quantumUsado;
	int remainingDistance;
	tRtaPosicion posRecurso;
} tPersonaje;

typedef struct {
	int   socket; 			 // Socket de conexion con el nivel
	char* nombre; 			 // Nombre del nivel
	t_queue* cListos; 		 // Cola de listos
	t_list*  lBloqueados; 	 // Lista de bloqueados (ordenada por orden de llegada)
	fd_set masterfds;
	tAlgoritmo algoritmo;
	int quantum;
	int delay;
	int maxSock;
	int rdDefault;			 // La remaining distance por default que manda el nivel
	pthread_cond_t hayPersonajes;
} tNivel;

typedef struct {
	tPersonaje *pPersonaje;
	tSimbolo   recursoEsperado;
} tPersonajeBloqueado;

typedef enum {
	byName,
	bySocket,
} tBusquedaPersonaje;

typedef enum {
	bySymbol,
	bySock,
	byRecursoBlock
} tBusquedaPersBlock;

//Hilos
void *orquestador(void *) ;
void *planificador(void *);

void imprimirLista(tNivel *pNivel, tPersonaje *pPersonaje);

//Delegar conexiones
void delegarConexion(fd_set *conjuntoDestino, fd_set *conjuntoOrigen, int iSocketADelegar, int *maxSockDestino);
void inicializarConexion(fd_set *master_planif, int *maxSock, int *sock);
void imprimirConexiones(fd_set *master_planif, int maxSock, char* host);

//Busquedas
int existeNivel(t_list * lNiveles, char* sLevelName);
int existPersonajeBlock(t_list *block, tSimbolo recurso, tBusquedaPersBlock);
int existePersonaje(t_list *pListaPersonajes, int valor, tBusquedaPersonaje criterio);
tPersonaje *getPersonaje(t_list *listaPersonajes, int valor, tBusquedaPersonaje criterio);
tPersonajeBloqueado *getPersonajeBlock(t_list *lBloqueados, int valor, tBusquedaPersBlock criterio);

//Constructores y destroyers
void agregarPersonaje(tNivel *pNivel, tSimbolo simbolo, int socket);
void crearNivel(t_list* lNiveles, tNivel* nivelNuevo, int socket, char *levelName, tInfoNivel *pInfoNivel);
void crearHiloPlanificador(pthread_t *pPlanificador, tNivel *nivelNuevo, t_list*);
tPersonajeBloqueado *createPersonajeBlock(tPersonaje *personaje, tSimbolo recurso);
tPersonaje *removePersonajeOfBlock(t_list *block, int indicePersonaje);

#endif /* PLATAFORMA_H_ */
