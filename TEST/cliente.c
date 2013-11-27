#include <ginyu/protocolo.h>
#include <ginyu/sockets.h>

int main(void) {
	puts("Se crea cliente");
	t_log *logger;
	logger = log_create("cliente.log", "CLIENTE", 1, LOG_LEVEL_TRACE);
	int iSocketComunicacion, bytesEnviados;
	unsigned short puerto = 2015;

	iSocketComunicacion = connectToServer("127.0.0.1", puerto, logger);

	/* EJEMPLO DE ENVIO DE AVISO */
	puts("Se envia aviso");
	tPaquete pkgAviso;
	pkgAviso.type   = PL_NIVEL_YA_EXISTENTE;
	pkgAviso.length = 0;
	bytesEnviados   = enviarPaquete(iSocketComunicacion, &pkgAviso, logger, "Se envia aviso nivel existente");

	printf("Se envian %d bytes\n", bytesEnviados);

	/* EJEMPLO DE ENVIO DE PAQUETE CON VARIOS DATOS */

	/* Armo lo que quiero mandar */
	tRtaPosicion posicion;
	tRtaPosicion* deserializado;
	posicion.posX = 33;
	posicion.posY = 88;

	/* Se crea el paquete */
	tPaquete pkgPosicion;
	serializarRtaPosicion(N_DATOS, posicion, &pkgPosicion);

	printf("Tipo: %d, largo: %d \n", pkgPosicion.type, pkgPosicion.length);
	deserializado = deserializarRtaPosicion(pkgPosicion.payload);
	printf("Se recibe X: %d e Y: %d \n", deserializado->posX, deserializado->posY);

	puts("Se envia paquete");
	bytesEnviados   = enviarPaquete(iSocketComunicacion, &pkgPosicion, logger, "Se envia aviso nivel existente");
	printf("Se envian %d bytes\n", bytesEnviados);


	tHandshakePers handshakePers;
	tHandshakePers* handDeserializado;
	handshakePers.simbolo = '@';
	handshakePers.nombreNivel = malloc(sizeof("unNombre"));
	strcpy(handshakePers.nombreNivel, "unNombre");
	/* Se crea el paquete */
	tPaquete pkgHandshake;
	serializarHandshakePers(P_HANDSHAKE, handshakePers, &pkgHandshake);

	printf("Tipo: %d, largo: %d \n", pkgHandshake.type, pkgHandshake.length);

	handDeserializado = deserializarHandshakePers(pkgHandshake.payload);
	printf("Se recibe simbolo: %d y nombre nivel: %s \n", handDeserializado->simbolo, handDeserializado->nombreNivel);

	puts("Se envia paquete");
	bytesEnviados   = enviarPaquete(iSocketComunicacion, &pkgHandshake, logger, "Se envia aviso nivel existente");
	printf("Se envian %d bytes\n", bytesEnviados);



	return 0;
}


