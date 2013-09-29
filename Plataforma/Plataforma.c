/*
 * Plataforma.c
 *
 *  Created on: 28/09/2013
 *      Author: utnso
 */
#include <stdlib.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include "socket_info.h"


int Socket_Create(int *listeningSocket){

	struct sockaddr_in socketInfo;
	int optval = 1;

	// Crear un socket:
	// AF_INET: Socket de internet IPv4
	// SOCK_STREAM: Orientado a la conexion, TCP
	// 0: Usar protocolo por defecto para AF_INET-SOCK_STREAM: Protocolo TCP/IPv4
	if ((*listeningSocket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("Socket Error");
		return EXIT_FAILURE;
	};

	// Hacer que el SO libere el puerto inmediatamente luego de cerrar el socket.
	setsockopt(*listeningSocket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

	socketInfo.sin_family = AF_INET;
	socketInfo.sin_addr.s_addr = DIRECCION; //Notar que aca no se usa inet_addr()
	socketInfo.sin_port = htons(PORT);

	// Vincular el socket con una direccion de red almacenada en 'socketInfo'.
		if (bind(*listeningSocket, (struct sockaddr*) &socketInfo, sizeof(socketInfo))!= 0) {
			perror("Error al bindear socket escucha");
			return EXIT_FAILURE;
		}
	return 0;
}

int Create_Level (int listeningSocket){
	/*
	 * Esta funcion debe:
	 * 1-Crear el hilo para montar un nuevo nivel
	 * 2-Montar el nuevo nivel y pasarle los parametros necesarios
	 */
	printf("TETA %d \n", listeningSocket);
	close(listeningSocket);
	return 0;
}

int main(){
	HANDSHAKE(0,9);
	int buffer;
	//Definicion de sockets
	int listeningSocket, NewConnection;
	int bytesRecibidos;

	Socket_Create(&listeningSocket);	//Falta verificar errores (!=0)

	// Escuchar nuevas conexiones entrantes.
	if (listen(listeningSocket, 10) != 0){
		perror("Error al poner a escuchar socket");
		return EXIT_FAILURE;
	};

	printf("Escuchando conexiones entrantes.\n");

	// Aceptar una nueva conexion entrante. Se genera un nuevo socket con la nueva conexion.
	if ((NewConnection = accept(listeningSocket, NULL, 0)) < 0) {
		perror("Error al aceptar conexion entrante");
		return EXIT_FAILURE;
	}

	//Recibe el codigo del proceso conectado. Realiza un HANDSHAKE, que llama a la funcion correspondiente.
	//Esta recepcion deberá ser ciclica.
	recv(NewConnection, &buffer, sizeof(int), 0);
	HANDSHAKE(buffer, NewConnection);	//Falta verificar errores (!=0)

	close(listeningSocket);

	return 0;
}
