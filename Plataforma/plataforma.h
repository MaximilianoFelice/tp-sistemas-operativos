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
	char name;
	t_list *recursos;
	bool ready;
	bool kill;
	int index;
	int sockID;
} personaje_t;

typedef struct {
	int sock; //socket con el que se maneja el nivel
	char nombre[16]; //nombre del nivel
	t_list* l_personajesRdy; //Cola de listos
	t_list* l_personajesBlk; //Cola de bloqueados
	t_list* l_personajesDie; //Cola de finalizados
	fd_set masterfds;
	bool hay_personajes;
	int maxSock;
	char algoritmo[16];
	int8_t Quantum;
	int delay;
} nivel_t;

typedef struct{
	bool condition;
	int sock;
} turno_t;

//Hilos
void *orquestador(unsigned short usPuerto);
void *planificador(void *);

bool estaMuerto(t_list * end, char name);
bool exist_personaje(t_list *list, char name_pj, int  *indice_pj);
bool sacarPersonajeDeListas(t_list *ready, t_list *block, int sock,  personaje_t *pjLevantador);
void desbloquearPersonajes(t_list* block, t_list *ready, personaje_t *pjLevantador, bool encontrado, char*nom_nivel, int proxPj);
void setTurno(turno_t *turno, bool condition_changed, int sock);
void turno(turno_t *turno, int delay, char *msjInfo);
void imprimirLista(char* nivel, t_list* rdy, t_list* blk, int cont);
void marcarPersonajeComoReady(t_list *ready, int sock);


//Delegar conexiones
void delegarConexion(fd_set *master_planif, fd_set *master_orq, int *sock, int *maxSock);
void inicializarConexion(fd_set *master_planif, int *maxSock, int *sock);
void imprimirConexiones(fd_set *master_planif, int maxSock, char* host);
void signal_personajes(bool *hay_personajes);
void wait_personajes(bool *hay_personajes);

//Busquedas
personaje_t *search_pj_by_name_with_return(t_list *lista, char name);
personaje_t *search_pj_by_socket_with_return(t_list *lista, int sock);
void search_pj_by_socket(t_list *lista, int sock, personaje_t *personaje);
void search_pj_by_name(t_list *lista, char name, personaje_t *personaje);
int search_index_of_pj_by_name(t_list *lista, char name, personaje_t *personaje);
int search_index_of_pj_by_socket(t_list *lista, int sock, personaje_t *personaje);
nivel_t * search_nivel_by_name_with_return(char *level_name);
bool existeNivel(nivel_t *nivel);

//Constructores y destroyers
void create_personaje(t_list *lista, char simbolo, int sock, int indice);
void create_level(nivel_t * nivelNuevo, int sock, char *level_name, char *alg, int q, int delay);
void crearHiloPlanificador(pthread_t *pPlanificador, nivel_t *nivelNuevo, t_list*);;

#endif /* PLATAFORMA_H_ */
