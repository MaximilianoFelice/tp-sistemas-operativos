#include <ginyu/protocolo.h>
#include <ginyu/sockets.h>

int main(void) {
	puts("Se crea server");
	t_log *logger;
	logger = log_create("server.log", "SERVER", 1, LOG_LEVEL_TRACE);
	unsigned short usPuerto = 2015;
	int maxSock;
	int iSocketEscucha;
	int iSocketComunicacion;

	fd_set setSocketsOrquestador;
	FD_ZERO(&setSocketsOrquestador);

	// Inicializacion de sockets y actualizacion del log
	iSocketEscucha = crearSocketEscucha(usPuerto, logger);

	FD_SET(iSocketEscucha, &setSocketsOrquestador);
	maxSock = iSocketEscucha;

	tMensaje tipoMensaje;
	char * sPayload;

	while (1) {
		puts("Escuchando");
		iSocketComunicacion = getConnection(&setSocketsOrquestador, &maxSock, iSocketEscucha, &tipoMensaje, &sPayload, logger);

		printf("Socket comunicacion: %d \n", iSocketComunicacion);

		if (iSocketComunicacion != -1) {

			switch (tipoMensaje) {
			case N_DATOS:
				puts("SE RECIBE POS");
				tRtaPosicion *posicion;
				posicion = deserializarRtaPosicion(sPayload);
				printf("Se recibe X: %d e Y: %d \n", posicion->posX, posicion->posY);
				free(sPayload);
				break;

			case PL_NIVEL_YA_EXISTENTE:
				puts("RECIBE AVISO DE NIVEL EXISTENTE");
				break;

			case P_HANDSHAKE:
				puts("SE RECIBE HAND");
				tHandshakePers *handshakePers;
				handshakePers = deserializarHandshakePers(sPayload);
				printf("Se recibe simbolo: %d y nombre nivel: %s \n", handshakePers->simbolo, handshakePers->nombreNivel);
				free(sPayload);
				break;
			}

		}//Fin del if

	}//Fin del while

	return 1;
}
