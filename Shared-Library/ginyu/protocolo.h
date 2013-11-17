/*
 * Sistemas Operativos - Super Mario Proc RELOADED.
 * Grupo       : C o no ser.
 * Nombre      : protocolo.h.
 * Descripcion : Este archivo contiene el protocolo de comunicacion entre procesos.
 */

#ifndef PROTOCOLO_H_
#define PROTOCOLO_H_

#include <stdint.h> //para los "int8_t"//////
#include <unistd.h> //para que no tire warning el close(i);
#include <string.h>

/*
 * Formato del tipo del paquete:
 * 		[emisor]_[mensaje]
 * Emisor:
 * 		N: Nivel
 * 		P: Personaje
 * 		PL: Plataforma
 *
 * 	aviso: significa que no manda nada
 */
typedef enum {
	N_HANDSHAKE,
	P_HANDSHAKE,
	PL_HANDSHAKE,
	P_POS_RECURSO,
	PL_POS_RECURSO,
	PL_OTORGA_TURNO,
	P_MOVIMIENTO,	 	// movimiento que hace el personaje
	PL_CONFIRMACION_MOV,  // Plataforma le manda al personaje
	PL_MOV_PERSONAJE, 	  // Plataforma le manda a nivel
	N_MUERTO_POR_ENEMIGO, // tSimbolo
	PL_MUERTO_POR_ENEMIGO,
	N_PERSONAJES_DEADLOCK, // tSimbolo (el personaje que ya se murio)
	PL_MUERTO_POR_DEADLOCK, // AVISO
	P_SIN_VIDAS,			// manda simbolo
	P_DESCONECTARSE_MUERTE, // AVISO
	PL_DESCONECTARSE_MUERTE,// AVISO
	P_DESCONECTARSE_FINALIZADO,// AVISO
	PL_DESCONECTARSE_MUERTE,// AVISO
	N_CONFIRMACION_ELIMINACION,// AVISO
	PL_CONFIRMACION_ELIMINACION,// AVISO
	PL_NIVEL_YA_EXISTENTE,// AVISO
	PL_NIVEL_INEXISTENTE,// AVISO
	N_ESTADO_PERSONAJE   // Los estados posibles despues del movimiento
} tMensaje;

typedef int8_t tSimbolo;

typedef enum {
	arriba,
	abajo,
	derecha,
	izquierda
} tDirMovimiento;

typedef enum {
	RR,
	SRDF
} tAlgoritmo;

typedef enum {
	bloqueado,
	otorgado,
	ok
} tEstado;


/*
 * Aca se definen los payloads que se van a mandar en los paquetes
 */
typedef struct {
	tSimbolo simbolo;
	char* nombreNivel;
} tHandshakePers;

typedef struct {
	int8_t delay;
	int8_t quantum;
	tAlgoritmo algoritmo;
	char* nombreNivel;
} tHandshakeNivel;

typedef struct {
	tSimbolo recurso;
	tSimbolo simbolo;
} tPregPosicion;

typedef struct {
	int8_t posX;
	int8_t posY;
} tRtaPosicion;

typedef struct {
	int8_t posX;
	int8_t posY;
} tRtaPosicion;

typedef char* tPersonajesDeadlock; // un array con todos los simbolos de los personajes que se bloquearon

int serializarHandshakePers(tHandshakePers *pHandshakePersonaje, char* payload);
tHandshakePers* deserializarHandshakePers(char * payload);

int serializarHandshakeNivel(tHandshakeNivel *pHandshakeNivel, char* payload);
tHandshakeNivel* deserializarHandshakeNivel(char * payload);

int serializarPregPosicion(tPregPosicion *pPregPosicion, char* payload);
tPregPosicion* deserializarPregPosicion(char * payload);

int serializarRtaPosicion(tRtaPosicion *pRtaPosicion, char* payload);
tRtaPosicion* deserializarRtaPosicion(char * payload);

int serializarMovimiento(tDirMovimiento *pDirMovimiento, char* payload);
tDirMovimiento* deserializarMovimiento(char * payload);

int serializarEstado(tEstado *pEstadoPersonaje, char* payload);
tEstado* deserializarEstado(char * payload);

#endif /* PROTOCOLO_H_ */
