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

#define POLL_TIMEOUT -1 // Negativo es que espera para siempre
#define POLL_NRO_FDS 2 // Uno para escuchar a la plataforma el otro para el inotify

//Posicion inicial del personaje.
#define INI_X 0
#define INI_Y 0

typedef struct {
	tNivel   * pNivel;
	tEnemigo * pEnemigo;
} tParamThreadEnemigo;

typedef struct {
	_Bool bloqueado;
	_Bool muerto; //Para que no lo intente matar dos veces seguidas
	_Bool listoParaPerseguir; //Se activa cuando se empieza a mover el personaje; es para que no lo persiga si el chabon todavia no empezo a moverse
	tSimbolo simbolo;
	t_list* recursos;
} tPersonaje;

typedef struct {
	int ID;
	int posX;
	int posY;
	pthread_t thread;
} tEnemigo;

typedef struct {
	char IP[INET_ADDRSTRLEN];
	unsigned short port;
	int delay;
	tAlgoritmo algPlanif;
	int valorAlgorimo;
} tInfoPlataforma;

typedef struct {
	char *nombre;
	int maxRows;
	int maxCols;
	int cantRecursos;
	int cantEnemigos;
	int sleepEnemigos;
	tInfoPlataforma plataforma;
} tNivel;

typedef struct {
	int recovery;
	int checkTime;
} tInfoInterbloqueo;

typedef struct {
	int x;
	int y;
} tPosicion;


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
void calcularMovimiento(tDirMovimiento direccion, int *posX, int *posY);
void matarPersonaje(tSimbolo *simboloItem);

//Mensajes
void confirmacionPlataforma(tPaquete *paquete, tMensaje tipoMensaje, char *msjInfo);
void solicitudError(tPaquete *paquete, tMensaje tipoMensaje, char *msjInfo);

//Busquedas e iteraciones de listas
tPersonaje *getPersonajeBySymbol(tSimbolo simbolo); //Busca en list_personajes
ITEM_NIVEL *getItemById(char id_victima); //Busca en list_items
void evitarRecurso(tEnemigo *enemigo);
ITEM_NIVEL *getVictima(tEnemigo *enemigo);


//Nivel
void CrearNuevoPersonaje(tPersonaje *pjNew, tSimbolo simbolo);
void levantarArchivoConf(char*);
void actualizarInfoNivel(char* argumento);
void liberarRecsPersonaje(char);
static void personaje_destroyer(tPersonaje *personaje);

//#undef NIVEL_H_
#endif //NIVEL_H_
