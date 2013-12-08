/*
 * Sistemas Operativos - Super Mario Proc RELOADED.
 * Grupo       : C o no ser.
 * Nombre      : nivel.h.
 * Descripcion : Este archivo contiene los prototipos de las
 * funciones usadas por el nivel.
 */


#ifndef NIVEL_H_
#define NIVEL_H_

#include "gui/tad_items.h"//<gui/tad_items.h> //Este importa GUI
#include <ginyu/protocolo.h>
#include <ginyu/config.h>
#include <ginyu/sockets.h>
#include <ginyu/list.h>
#include <ginyu/log.h>

#include <sys/inotify.h>
#include <sys/poll.h>

#include <time.h>
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
#include <math.h>

//Posicion inicial del personaje.
#define INI_X 0
#define INI_Y 0

typedef struct {
	_Bool bloqueado;
	tSimbolo simbolo;
	t_list* recursos;
} pers_t;

typedef struct {
	int num_enemy;
	int sockP;
	int posX;
	int posY;
} enemigo_t;

typedef struct {
	pthread_t thread_enemy;
	enemigo_t enemy;
} threadEnemy_t;

typedef struct{
	int cantidadInstancias;
	char simbolo;
}t_caja;

//Señales
void cerrarNivel(char*);
void cerrarForzado(int sig);
//Enemigos
void *enemigo(void * args);
void *deteccionInterbloqueo (void *parametro);
void actualizaPosicion(int *contMovimiento, int *posX, int *posY);

//Nivel
void matarPersonaje(ITEM_NIVEL *item);
void levantarArchivoConf(char*);
void actualizarInfoNivel(char* argumento);
void liberarRecsPersonaje(char);
void liberarRecsPersonaje2(char id);
static void personaje_destroyer(pers_t *personaje);
//static void recurso_destroyer(char *recurso);

//#undef NIVEL_H_
#endif //NIVEL_H_
