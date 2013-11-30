/*
 * Sistemas Operativos - Super Mario Proc RELOADED.
 * Grupo       : C o no ser.
 * Nombre      : protocolo.h.
 * Descripcion : Este archivo contiene el protocolo de comunicacion entre procesos.
 */

#ifndef PROTOCOLO_H_
#define PROTOCOLO_H_

#include <stdlib.h>
#include <stdint.h> //para los "int8_t"
#include <string.h>

#define MAX_BUFFER 1024

typedef struct {
	int8_t  type;
	int16_t length;
} __attribute__ ((__packed__)) tHeader;

typedef struct {
	int8_t  type;
	int16_t length;
	char    payload[MAX_BUFFER];
} __attribute__ ((__packed__)) tPaquete;

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
	/* Mensajes de la plataforma */
	PL_HANDSHAKE,
	PL_PERSONAJE_REPETIDO,
	PL_POS_RECURSO,
	PL_OTORGA_TURNO,			// Plataforma le manda al personaje
	PL_CONFIRMACION_MOV,  		// Plataforma le manda al personaje
	PL_MOV_PERSONAJE, 	  		// Plataforma le manda a nivel
	PL_DESCONECTARSE_MUERTE,	// AVISO
	PL_MUERTO_POR_ENEMIGO,
	PL_MUERTO_POR_DEADLOCK, 	// AVISO
	PL_CONFIRMACION_ELIMINACION,// AVISO
	PL_NIVEL_YA_EXISTENTE,		// AVISO
	PL_NIVEL_INEXISTENTE,		// AVISO
	PL_SOLICITUD_RECURSO,
	PL_RECURSO_INEXISTENTE,		// AVISO
	PL_RECURSO_OTORGADO,		// AVISO
	PL_DESCONEXION_PERSONAJE,
	/* Mensajes del nivel */
	N_HANDSHAKE,
	N_PERSONAJE_AGREGADO,		//Se conecta un nuevo personaje correctamente
	N_PERSONAJE_ERROR,			//El personaje ya existia
	N_CONFIRMACION_ELIMINACION,	// AVISO
	N_MUERTO_POR_ENEMIGO, 		// tSimbolo
	N_PERSONAJES_DEADLOCK, 		// tSimbolo (el personaje que ya se murio)
	N_ESTADO_PERSONAJE,   		// Los estados posibles despues del movimiento
	N_POS_RECURSO,
	N_DATOS,
	N_ACTUALIZACION_CRITERIOS,
	N_ENTREGA_RECURSO,
	N_CONFIRMACION_MOV,
	/* Mensajes del personaje */
	P_HANDSHAKE,
	P_MOVIMIENTO,	 			// movimiento que hace el personaje
	P_POS_RECURSO,				//solicita la posicion del recurso
	P_SIN_VIDAS,				// manda simbolo
	P_DESCONECTARSE_MUERTE, 	// AVISO
	P_DESCONECTARSE_FINALIZADO,	// AVISO
	P_SOLICITUD_RECURSO,		//solicita el recurso en si
	P_FIN_TURNO,				//AVISO a plataforma
	P_FIN_PLAN_NIVELES,			//AVISO cuando se terminan todos los niveles
	/* Mensajes comunes */
	DESCONEXION,
	NO_SE_OBTIENE_RESPUESTA
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
	tSimbolo simbolo;
} tHandshakeNivel;

typedef struct {
	uint32_t delay;
	int8_t quantum;
	tAlgoritmo algoritmo;
} tInfoNivel;

typedef struct {
	tSimbolo recurso;
	tSimbolo simbolo;
} tPregPosicion;

typedef struct {
	int8_t posX;
	int8_t posY;
} tRtaPosicion;

typedef struct {
	tSimbolo simbolo;
	tDirMovimiento direccion;
} tMovimientoPers;

typedef char* tPersonajesDeadlock; // un array con todos los simbolos de los personajes que se bloquearon

int serializarHandshakePers(tMensaje tipoMensaje, tHandshakePers handshakePersonaje, tPaquete* pPaquete);
tHandshakePers* deserializarHandshakePers(char * payload);

int serializarHandshakeNivel(tMensaje tipoMensaje, tHandshakeNivel simbolo, tPaquete* pPaquete);
tHandshakeNivel* deserializarHandshakeNivel(char * payload);

int serializarInfoNivel(tMensaje tipoMensaje, tInfoNivel infoNivel, tPaquete* pPaquete);
tInfoNivel* deserializarInfoNivel(char * payload);

int serializarPregPosicion(tMensaje tipoMensaje, tPregPosicion pregPosicion, tPaquete* pPaquete);
tPregPosicion* deserializarPregPosicion(char * payload);

int serializarRtaPosicion(tMensaje tipoMensaje, tRtaPosicion pRtaPosicion, tPaquete* pPaquete);
tRtaPosicion* deserializarRtaPosicion(char * payload);

int serializarRtaPosicion(tMensaje tipoMensaje, tRtaPosicion pRtaPosicion, tPaquete* pPaquete);
tRtaPosicion* deserializarRtaPosicion(char * payload);

int serializarMovimiento(tMensaje tipoMensaje, tDirMovimiento dirMovimiento, tPaquete* pPaquete);
tDirMovimiento* deserializarMovimiento(char * payload);

int serializarMovimientoPers(tMensaje tipoMensaje, tMovimientoPers movimientoPers, tPaquete* pPaquete);
tMovimientoPers* deserializarMovimientoPers(char * payload);

int serializarEstado(tMensaje tipoMensaje, tEstado estadoPersonaje, tPaquete* pPaquete);
tEstado* deserializarEstado(char * payload);

int serializarSimbolo(tMensaje tipoMensaje, tSimbolo simbolo, tPaquete* pPaquete);
tSimbolo* deserializarSimbolo(char * payload);

#endif /* PROTOCOLO_H_ */
