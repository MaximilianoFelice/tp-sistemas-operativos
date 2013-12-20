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
	int x;
	int y;
} tPosicion;


typedef struct {
	_Bool bloqueado;
	_Bool muerto; //Para que no lo intente matar dos veces seguidas
	_Bool marcado;
	tPosicion posicion;
	tSimbolo simbolo;
	t_list* recursos;
} tPersonaje;


typedef struct {
	char IP[INET_ADDRSTRLEN];
	unsigned short port;
	int delay;
	tAlgoritmo algPlanif;
	int valorAlgorimo;
	unsigned int socket;
} tInfoPlataforma;


typedef struct {
	int recovery;
	int checkTime;
} tInfoInterbloqueo;


typedef struct {
	char *nombre;
	int maxRows;
	int maxCols;
	int cantRecursos;
	int cantEnemigos;
	int sleepEnemigos;
	tInfoPlataforma plataforma;
	tInfoInterbloqueo deadlock;
} tNivel;

typedef struct {
	int cantidadInstancias;
	char simbolo;
} t_caja;


typedef struct {
	int ID;
	int posX;
	int posY;
	pthread_t thread;
	tNivel *pNivel;
} tEnemigo;

//Acciones de los mensajes
void handshakeConPlataforma(tNivel *pNivel);
void conexionPersonaje(int iSocket, char *sPayload);
void movimientoPersonaje(tNivel *pNivel, int iSocket, char *sPayload);
void posicionRecurso(tNivel *pNivel, int iSocket, char *sPayload);
void solicitudRecurso(tNivel *pNivel, int iSocket, char *sPayload);
void desconexionPersonaje(tNivel *pNivel, char *sPayload) ;
void escucharConexiones(tNivel *pNivel, char* configFilePath);
void levantarArchivoConf(char* pathConfigFile, tNivel *pNivel);
void actualizarInfoNivel(tNivel *pNivel, int iSocket, char* configFilePath);
void notificacionAPlataforma(int iSocket, tMensaje tipoMensaje, char *msjInfo);
void liberarRecursosPersonajeMuerto(tNivel *pNivel, char *sPayload);
void liberarRecursos(tNivel *pNivel, tDesconexionPers *persDesconectado);
void desbloquearPersonajes(tNivel *pNivel, tDesconexionPers *persDesconectado);

//Señales
void cerrarNivel(char*);
void cerrarForzado(int sig);

//Hilo deadlock
void *deteccionInterbloqueo(void* parametro);
_Bool tieneLoQueNecesito(tPersonaje* pPersonaje1, tPersonaje* pPersonaje2);

//Hilo Enemigos
void *enemigo(void * args);
void actualizaPosicion(tDirMovimiento dirMovimiento, int *posX, int *posY);
void calcularMovimiento(tNivel *pNivel, tDirMovimiento direccion, int *posX, int *posY);
void matarPersonaje(tNivel *pNivel, tSimbolo *simboloItem, tMensaje tipoMensaje);
void evitarRecurso(tEnemigo *enemigo);
void evitarOrigen(tEnemigo *enemigo);
tPersonaje *asignarVictima(tEnemigo *enemigo);
int calcularDistancia(tEnemigo *enemigo, int posX, int posY);
_Bool analizarMovimientoDeEnemigo();
_Bool esUnPersonaje(ITEM_NIVEL *item);
_Bool acercarmeALaVictima(tEnemigo *enemigo, tPersonaje *personaje, tDirMovimiento *dirMovimiento);
_Bool estoyArriba(tEnemigo *enemigo, tPersonaje *persVictima);
_Bool hayAlgunEnemigoArriba(tNivel *pNivel, int posPerX, int posPerY);
bool estaArribaDeUnRecurso(tPersonaje *personaje);

//Busquedas e iteraciones de listas
tPersonaje *getPersonajeBySymbol(tSimbolo simbolo); //Busca en list_personajes
ITEM_NIVEL *getItemById(char id_victima); //Busca en list_items

//Constructores . Destroyers
void crearNuevoPersonaje (tSimbolo simbolo);
static void personaje_destroyer(tPersonaje *personaje);
void crearEnemigos(tNivel *nivel);

//#undef NIVEL_H_
#endif //NIVEL_H_
