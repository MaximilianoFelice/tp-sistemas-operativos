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

