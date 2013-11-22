/*
 * Sistemas Operativos - Super Mario Proc RELOADED.
 * Grupo       : C o no ser.
 * Nombre      : personaje.h.
 * Descripcion : Este archivo contiene los prototipos de las
 * funciones usadas por el personaje.
 */

#ifndef PERSONAJE_H_
#define PERSONAJE_H_

#include <commons/collections/list.h>
#include <commons/config.h>
#include <commons/string.h>
#include <commons/log.h>

#include <ginyu/config.h>
#include <ginyu/list.h>
#include <ginyu/log.h>
#include <ginyu/sockets.h>
#include <ginyu/protocolo.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>

typedef struct Nivel {
	char *nomNivel;
	t_list *Objetivos;
	int num_of_thread;
} nivel_t;

typedef struct ThreadNivel{
	pthread_t thread;
	nivel_t nivel;
} threadNivel_t;

typedef struct PersonajeGlobal{
	char simbolo;
	int vidas;
	int vidasMaximas;
	t_list *listaNiveles;
	char * nombre;

} personajeGlobal_t;

typedef struct PersonajeIndividual{
	int socketPlataforma;
	int currObj; //objetivo actual
	int posX;
	int posY;
	int posRecursoX;
	int posRecursoY;
	nivel_t *nivelQueJuego;

} personajeIndividual_t;



void *jugar(void *argumentos);
int calculaMovimiento(personajeIndividual_t personajePorNivel);
void handshake_plataforma(personajeIndividual_t *personajePorNivel);
void actualizaPosicion(int movimiento, personajeIndividual_t *personajePorNivel);
void morir(char* causaMuerte, personajeIndividual_t personajePorNivel);
bool devolverRecursos(int *socketPlataforma, message_t *message);
void pedirPosicionRecurso(personajeIndividual_t* personajePorNivel, char recurso);
void destruirArchivoConfiguracion(t_config *configPersonaje);
void cargarArchivoConfiguracion(char* archivoConfiguracion);
void obtenerIpYPuerto(char *dirADividir, char * ip,  char * puerto);
bool validarSenial(bool *murioPersonaje);
bool estaMuerto(int8_t, bool *);
void cerrarConexiones(int * socketPlataforma);
void aumentarVidas();
void morirSenial();
void restarVidas();
static void nivel_destroyer(nivel_t* nivel);

#endif /* PERSONAJE_H_ */
