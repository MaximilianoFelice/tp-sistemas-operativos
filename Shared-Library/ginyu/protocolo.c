/*
 * Sistemas Operativos - Super Mario Proc RELOADED.
 * Grupo       : C o no ser.
 * Nombre      : protocolo.c.
 * Descripcion : Este archivo contiene la implementacion del protocolo de comunicacion entre procesos.
 */
#include "protocolo.h"

int serializarHandshakePers(tHandshakePers *pHandshakePersonaje, char* payload) {
	int offset   = 0;
	int tmp_size = 0;
	int length;

	tmp_size = sizeof(pHandshakePersonaje->simbolo);
	memcpy(payload, &pHandshakePersonaje->simbolo, tmp_size);

	offset   = tmp_size;
	tmp_size = strlen(pHandshakePersonaje->nombreNivel) + 1;
	memcpy(payload + offset, pHandshakePersonaje->nombreNivel, tmp_size);

	length = offset + tmp_size;

	return length;
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


int serializarInfoNivel(tInfoNivel *pInfoNivel, char* payload) {
	int offset   = 0;
	int tmp_size = 0;
	int length;

	tmp_size = sizeof(pInfoNivel->delay);
	memcpy(payload, &pInfoNivel->delay, tmp_size);

	offset   = tmp_size;
	tmp_size = sizeof(pInfoNivel->quantum);
	memcpy(payload, &pInfoNivel->quantum, tmp_size);

	offset   =+ tmp_size;
	tmp_size = sizeof(pInfoNivel->algoritmo);
	memcpy(payload, &pInfoNivel->algoritmo, tmp_size);

	length = offset + tmp_size;

	return length;
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


int serializarPregPosicion(tPregPosicion *pPregPosicion, char* payload) {
	int offset   = 0;
	int tmp_size = 0;
	int length;

	tmp_size = sizeof(pPregPosicion->recurso);
	memcpy(payload, &pPregPosicion->recurso, tmp_size);

	offset   = tmp_size;
	tmp_size = sizeof(pPregPosicion->simbolo);
	memcpy(payload, &pPregPosicion->simbolo, tmp_size);

	length = offset + tmp_size;

	return length;
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


int serializarRtaPosicion(tRtaPosicion *pRtaPosicion, char* payload) {
	int offset   = 0;
	int tmp_size = 0;
	int length;

	tmp_size = sizeof(pRtaPosicion->posX);
	memcpy(payload, &pRtaPosicion->posX, tmp_size);

	offset   = tmp_size;
	tmp_size = sizeof(pRtaPosicion->posY);
	memcpy(payload, &pRtaPosicion->posY, tmp_size);

	length = offset + tmp_size;

	return length;
}

tRtaPosicion * deserializarRtaPosicion(char * payload) {
	tRtaPosicion *pRtaPosicion = malloc(sizeof(tRtaPosicion));
	int offset   = 0;
	int tmp_size = 0;

//	tmp_size = sizeof(int8_t);
//	memcpy(&pRtaPosicion->posX, *payload, tmp_size);
//
//	offset   = tmp_size;
//	tmp_size = sizeof(int8_t);
//	memcpy(&pRtaPosicion->posY, *payload + offset, tmp_size);
	pRtaPosicion = malloc(sizeof(pRtaPosicion));
	pRtaPosicion->posX = payload[0];
	pRtaPosicion->posY = payload[1];
	return pRtaPosicion;
}


int serializarMovimiento(tDirMovimiento *pDirMovimiento, char* payload) {
	int length;

	length = sizeof(pDirMovimiento);
	memcpy(payload, &pDirMovimiento, length);

	return length;
}

tDirMovimiento* deserializarMovimiento(char * payload) {
	tDirMovimiento *pDirMovimiento = malloc(sizeof(tDirMovimiento));
	int tmp_size = 0;

	tmp_size = sizeof(int8_t);
	memcpy(&pDirMovimiento, payload, tmp_size);

	return pDirMovimiento;
}


int serializarEstado(tEstado *pEstadoPersonaje, char* payload) {
	int length;

	length = sizeof(pEstadoPersonaje);
	memcpy(payload, &pEstadoPersonaje, length);

	return length;
}

tEstado* deserializarEstado(char * payload) {
	tEstado *pEstadoPersonaje = malloc(sizeof(tEstado));
	int tmp_size = 0;

	tmp_size = sizeof(int8_t);
	memcpy(&pEstadoPersonaje, payload, tmp_size);

	return pEstadoPersonaje;
}
