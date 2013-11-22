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
	tPaquete pkgPosicion;
	tRtaPosicion posicion;
	posicion.posX = 43;
	posicion.posX = 60;

	/* Se crea el paquete */
	pkgPosicion.type = N_DATOS;
	pkgPosicion.length = serializarRtaPosicion(&posicion, pkgPosicion.payload);
	puts("Se envia paquete");
	bytesEnviados   = enviarPaquete(iSocketComunicacion, &pkgPosicion, logger, "Se envia aviso nivel existente");
	printf("Se envian %d bytes\n", bytesEnviados);

	return 0;
}


