/*
 * socket_info.h
 *
 *  Created on: 28/09/2013
 *      Author: utnso
 */

#ifndef SOCKET_INFO_H_
#define SOCKET_INFO_H_

#define DIRECCION INADDR_ANY	//Cualquier direccion conectada

#define PORT 27864
#define BUFF_SIZE 1024

typedef int ID;

struct character_properties{
	int LALA;
	//PROPIEDADES DE CHAR AQUI
};

int Create_Level(int);

int Add_Char(int);

struct function_code{
	ID ID;
	int (*function)();
} function_by_code[] = {{0, &Create_Level},{1,&Create_Level}};

#define HANDSHAKE(n,newConnection) (function_by_code[n].function)(newConnection);

#endif /* SOCKET_INFO_H_ */

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
