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
#include <ginyu/epoll.h>
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

#define PUERTO_ORQ 5000
#define PUERTO_PLAN 5050


typedef struct {
	char name;
	t_list *recursos;
	bool ready;
	bool kill;
	int index;
	int posX;
	int posY;
	int sockID;
} personaje_t;


typedef char recurso;

typedef struct {
	int sock; //socket con el que se maneja el nivel
	int puertoPlan; //planificador del nivel
	char nombre[16]; //nombre del nivel
	t_list* l_personajesRdy; //Cola de listos
	t_list* l_personajesBlk; //Cola de bloqueados
	t_list* l_personajesDie; //Cola de finalizados
	char algoritmo[16];
	int8_t quantum;
	int retardo;
} nivel_t;


typedef struct {
	pthread_t hiloPlan;
	nivel_t nivel;
} threadPlanificador_t;

void *orquestador();
void *planificador(void *);
bool estaMuerto(t_list * end, char name);
bool exist_personaje(t_list *list, char name_pj, int  *indice_pj);
bool buscarPersonajePor(t_list *ready, t_list *block, int i, bool(*condition)(void*), personaje_t *);
void desbloquearPersonajes(t_list* block, t_list *ready, personaje_t *pjLevantador, bool encontrado, char*nom_nivel, int proxPj);
int  encontreNivel(nivel_t *nivelAux, char *nameToSearch);
void imprimirLista(char* nivel, t_list* rdy, t_list* blk, int cont);

#endif /* PLATAFORMA_H_ */
