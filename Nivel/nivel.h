/*
 * Sistemas Operativos - Super Mario Proc RELOADED.
 * Grupo       : C o no ser.
 * Nombre      : nivel.h.
 * Descripcion : Este archivo contiene los prototipos de las
 * funciones usadas por el nivel.
 */


#ifndef NIVEL_H_
#define NIVEL_H_

#include <gui/tad_items.h> //Este importa GUI
#include <ginyu/protocolo.h>
#include <ginyu/config.h>
#include <ginyu/sockets.h>
#include <ginyu/list.h>
#include <ginyu/log.h>

#include <sys/inotify.h>

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
	_Bool blocked;
	_Bool marcado;
	char simbolo;
	_Bool muerto;
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

typedef enum movimientos_x{
	izquierda=102,
	derecha=103,
} enum_mov_x;

typedef enum movimientos_y{
	arriba=100,
	abajo=101,
} enum_mov_y;

typedef struct {
	enum_mov_x type_mov_x;
	enum_mov_y type_mov_y;
	bool in_x;
	bool in_y;
} mov_t;

//Señales
void cerrarNivel(char*);
void cerrarForzado(int sig);
//Enemigos
void *enemigo(void * args);
_Bool personajeMuerto(t_list *list_personajes, char name);
void KillPersonaje(t_list *list_personajes, char name);
void moverme(int *victimaX, int *victimaY, int *posX, int *posY, mov_t *movimiento);
void validarPosSobreRecurso(t_list *list_items, mov_t movimiento, int *posX,int *posY);
void matar(enemigo_t *enemigo, pers_t *pjVictima, int indice, char*ip_plataforma, int puertoPlan);
void actualizaPosicion(int *contMovimiento, int *posX, int *posY);
bool hayAlgunEnemigoArriba(int posPerX, int posPerY);
pers_t* hayAlgunEnemigoArribaDeAlgunPersonaje();
//Nivel
void liberarRecursos(pers_t *personajeAux, int index_l_personajes);
static void personaje_destroyer(pers_t *personaje);
static void recurso_destroyer(char *recurso);

#endif /* NIVEL_H_ */
