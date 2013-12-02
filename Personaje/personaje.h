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
#include <stdbool.h>
#include <pthread.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
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
	tSimbolo simbolo;
	int vidas;
	int vidasMaximas;
	unsigned short puertoOrquestador;
	t_list *listaNiveles;
	char * nombre;
	char * ipOrquestador;

} personajeGlobal_t;

typedef struct PersonajeIndividual{
	int socketPlataforma;
	tSimbolo recursoActual; //objetivo actual
	int posX;
	int posY;
	int posRecursoX;
	int posRecursoY;
	nivel_t *nivelQueJuego;

} personajeIndividual_t;


void notificarFinPlanNiveles(int socketOrquestador);

void destruirArchivoConfiguracion(t_config *configPersonaje);

void cargarArchivoConfiguracion(char* archivoConfiguracion);

void obtenerIpYPuerto(char *dirADividir, char * ip,  char * puerto);

static void nivel_destroyer(nivel_t*nivel);

void *jugar(void *args);

void desconectarPersonaje(personajeIndividual_t* personajePorNivel);

void manejarDesconexiones(personajeIndividual_t* personajePorNivel, bool murioPersonaje);//, bool* finalice)

bool personajeEstaMuerto(bool murioPersonaje);

bool conseguiRecurso(personajeIndividual_t personajePorNivel);

void moverAlPersonaje(personajeIndividual_t* personajePorNivel);

void solicitarRecurso(int socketPlataforma, char * recurso);

tDirMovimiento calcularYEnviarMovimiento(personajeIndividual_t *personajePorNivel);

void recibirMensajeTurno(int socketPlataforma);

void pedirPosicionRecurso(personajeIndividual_t* personajePorNivel, char* recurso);

void reintentarSolicitudRecurso(personajeIndividual_t* personajePorNivel, tPaquete* pkgHandshake, char* recurso);

void handshake_plataforma(personajeIndividual_t* personajePorNivel);

void reintentarHandshake(int socketPlataforma, tPaquete* pkgHandshake);

void cerrarConexiones(int * socketPlataforma);

//TODO te descomente esto porque despues usabas esta funcion en otro lado y tiraba error
//void devolverRecursosPorFinNivel(int socketPlataforma);

int calculaMovimiento(personajeIndividual_t personajePorNivel);

void actualizaPosicion(tDirMovimiento* movimiento, personajeIndividual_t **personajePorNivel);

void sig_aumentar_vidas();

void restarVida();

void muertoPorSenial();

void matarHilosYDesconectar();

void reiniciarJuego();




#endif /* PERSONAJE_H_ */
