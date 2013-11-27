/*
 * Sistemas Operativos - Super Mario Proc RELOADED.
 * Grupo       : C o no ser.
 * Nombre      : protocolo.c.
 * Descripcion : Este archivo contiene la implementacion del protocolo de comunicacion entre procesos.
 */
#include "protocolo.h"

int serializarHandshakePers(tMensaje tipoMensaje, tHandshakePers handshakePersonaje, tPaquete* pPaquete) {
	int offset   = 0;
	int tmp_size = 0;

	pPaquete->type = tipoMensaje;

	tmp_size = sizeof(handshakePersonaje.simbolo);
	memcpy(pPaquete->payload, &handshakePersonaje.simbolo, tmp_size);

	offset   = tmp_size;
	tmp_size = strlen(handshakePersonaje.nombreNivel) + 1;
	memcpy(pPaquete->payload + offset, handshakePersonaje.nombreNivel, tmp_size);

	pPaquete->length = offset + tmp_size;

	return EXIT_SUCCESS;
}

tHandshakePers* deserializarHandshakePers(char * payload) {
	tHandshakePers *pHandshakePersonaje = malloc(sizeof(tHandshakePers));
	int offset   = 0;
	int tmp_size = 0;

	tmp_size = sizeof(tSimbolo);
	memcpy(&pHandshakePersonaje->simbolo, payload, tmp_size);

	offset = tmp_size;
	for (tmp_size = 1; (payload + offset)[tmp_size-1] != '\0'; tmp_size++);
	pHandshakePersonaje->nombreNivel = malloc(tmp_size);
	memcpy(pHandshakePersonaje->nombreNivel, payload + offset, tmp_size);

	return pHandshakePersonaje;
}


int serializarInfoNivel(tMensaje tipoMensaje, tInfoNivel infoNivel, tPaquete* pPaquete) {
	int offset   = 0;
	int tmp_size = 0;

	pPaquete->type = tipoMensaje;

	tmp_size = sizeof(infoNivel.delay);
	memcpy(pPaquete->payload, &infoNivel.delay, tmp_size);

	offset   = tmp_size;
	tmp_size = sizeof(infoNivel.quantum);
	memcpy(pPaquete->payload + offset, &infoNivel.quantum, tmp_size);

	offset   =+ tmp_size;
	tmp_size = sizeof(infoNivel.algoritmo);
	memcpy(pPaquete->payload + offset, &infoNivel.algoritmo, tmp_size);

	pPaquete->length = offset + tmp_size;

	return EXIT_SUCCESS;
}

tInfoNivel * deserializarInfoNivel(char * payload) {
	tInfoNivel *pInfoNivel = malloc(sizeof(tInfoNivel));
	int offset   = 0;
	int tmp_size = 0;

	tmp_size = sizeof(int8_t);
	memcpy(&pInfoNivel->delay, payload, tmp_size);

	offset   = tmp_size;
	tmp_size = sizeof(int8_t);
	memcpy(&pInfoNivel->quantum, payload + offset, tmp_size);

	offset   =+ tmp_size;
	tmp_size = sizeof(tAlgoritmo);
	memcpy(&pInfoNivel->algoritmo, payload + offset, tmp_size);

	return pInfoNivel;
}


int serializarPregPosicion(tMensaje tipoMensaje, tPregPosicion pregPosicion, tPaquete* pPaquete) {
	int offset   = 0;
	int tmp_size = 0;

	pPaquete->type = tipoMensaje;

	tmp_size = sizeof(pregPosicion.recurso);
	memcpy(pPaquete->payload, &(pregPosicion.recurso), tmp_size);

	offset   = tmp_size;
	tmp_size = sizeof(pregPosicion.simbolo);
	memcpy(pPaquete->payload + offset, &(pregPosicion.simbolo), tmp_size);

	pPaquete->length = offset + tmp_size;

	return EXIT_SUCCESS;
}

tPregPosicion * deserializarPregPosicion(char * payload) {

	tPregPosicion *pPregPosicion = malloc(sizeof(tPregPosicion));
	int offset   = 0;
	int tmp_size = 0;

	tmp_size = sizeof(tSimbolo);
	memcpy(&pPregPosicion->recurso, payload, tmp_size);

	offset   = tmp_size;
	tmp_size = sizeof(tSimbolo);
	memcpy(&pPregPosicion->simbolo, payload + offset, tmp_size);

	return pPregPosicion;
}


int serializarRtaPosicion(tMensaje tipoMensaje, tRtaPosicion rtaPosicion, tPaquete* pPaquete) {

	int offset   = 0;
	int tmp_size = 0;

	pPaquete->type = tipoMensaje;

	tmp_size = sizeof(rtaPosicion.posX);
	memcpy(pPaquete->payload, &(rtaPosicion.posX), tmp_size);

	offset   = tmp_size;
	tmp_size = sizeof(rtaPosicion.posY);
	memcpy(pPaquete->payload + offset, &(rtaPosicion.posY), tmp_size);

	pPaquete->length = offset + tmp_size;

	return EXIT_SUCCESS;
}

tRtaPosicion * deserializarRtaPosicion(char * payload) {

	tRtaPosicion *pRtaPosicion = malloc(sizeof(tRtaPosicion));
	int offset   = 0;
	int tmp_size = 0;

	tmp_size = sizeof(pRtaPosicion->posX);
	memcpy(&pRtaPosicion->posX, payload, tmp_size);

	offset   = tmp_size;
	tmp_size = sizeof(pRtaPosicion->posY);
	memcpy(&pRtaPosicion->posY, payload + offset, tmp_size);

	return pRtaPosicion;
}


int serializarMovimiento(tMensaje tipoMensaje, tDirMovimiento dirMovimiento, tPaquete* pPaquete) {

	pPaquete->type = tipoMensaje;

	pPaquete->length = sizeof(dirMovimiento);
	memcpy(pPaquete->payload, &dirMovimiento, pPaquete->length);

	return EXIT_SUCCESS;
}

tDirMovimiento* deserializarMovimiento(char * payload) {

	tDirMovimiento *pDirMovimiento = malloc(sizeof(tDirMovimiento));
	int tmp_size = 0;

	tmp_size = sizeof(int8_t);
	memcpy(&pDirMovimiento, payload, tmp_size);

	return pDirMovimiento;
}

int serializarMovimientoPers(tMensaje tipoMensaje, tMovimientoPers movimientoPers, tPaquete* pPaquete){

	pPaquete->type = tipoMensaje;

	pPaquete->length = sizeof(movimientoPers);
	memcpy(pPaquete->payload, &movimientoPers, pPaquete->length);

	return EXIT_SUCCESS;

}

tMovimientoPers* deserializarMovimientoPers(char * payload){

	tMovimientoPers *movimientoPers = malloc(sizeof(tMovimientoPers));
	int tmp_size = 0;

	tmp_size = sizeof(int8_t);
	memcpy(&movimientoPers, payload, tmp_size);

	return movimientoPers;
}


int serializarEstado(tMensaje tipoMensaje, tEstado estadoPersonaje, tPaquete* pPaquete) {

	pPaquete->type = tipoMensaje;

	pPaquete->length = sizeof(estadoPersonaje);
	memcpy(pPaquete->payload, &estadoPersonaje, pPaquete->length);

	return EXIT_SUCCESS;
}

tEstado* deserializarEstado(char * payload) {

	tEstado *pEstadoPersonaje = malloc(sizeof(tEstado));
	int tmp_size = 0;

	tmp_size = sizeof(int8_t);
	memcpy(&pEstadoPersonaje, payload, tmp_size);

	return pEstadoPersonaje;
}


int serializarSimbolo(tMensaje tipoMensaje, tSimbolo simbolo, tPaquete* pPaquete) {

	pPaquete->type = tipoMensaje;

	pPaquete->length = sizeof(simbolo);
	memcpy(pPaquete->payload, &simbolo, pPaquete->length);

	return EXIT_SUCCESS;
}

tSimbolo* deserializarSimbolo(char * payload) {

	tSimbolo *pSimbolo = (tSimbolo*)malloc(sizeof(tSimbolo));
	int tmp_size = 0;

	tmp_size = sizeof(tSimbolo);
	memcpy(&pSimbolo, payload, tmp_size);

	return pSimbolo;
}
