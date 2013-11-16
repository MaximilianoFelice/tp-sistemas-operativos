
#ifndef PROTOCOLO_H_
#define PROTOCOLO_H_


#include <stdint.h> //para los "int8_t"//////
#include <unistd.h> //para que no tire warning el close(i);
#include <string.h>

//HandShakes
#define SALUDO 1
#define INFO 2
#define INFO_PLANIFICADOR 22
#define WHATS_UP 24

//Acciones
#define MOVIMIENTO 3
#define POSICION_RECURSO 4
#define TURNO 7
#define SALIR 8
#define NADA 20
#define REPETIDO 99

//Estado
#define BLOCK 10
#define OTORGADO 11
#define MUERTO_DEADLOCK 78
#define MUERTO_ENEMIGOS 77

//Emisor
#define NIVEL 16
#define PERSONAJE 17
#define PLATAFORMA 80

typedef enum {
	N_HANDSHAKE
} tMensaje;

typedef enum {
	arriba,
	abajo,
	derecha,
	izquierda
} tMovimientos;

typedef enum {
	RR,
	SRDF
} tAlgoritmo;

typedef struct{
	int8_t type;
	int8_t detail;
	int8_t detail2;
	char name;
} message_t;

typedef struct{// Desde el punto de vista del orquestador
	int8_t type; 	// Nivel					-	Personaje
	int8_t detail;	// Saludo/Salir
	int	port; // Recibe puerto del nivel	-	Manda puerto del nivel
	char ip[16]; 	// Recibe Ip				-	Manda Ip
	char name[16]; // Nombre de nivel			-	Nombre de nivel pedido
} orq_t;

//Para el planificador
message_t armarMsj_return(char name, int8_t type, int8_t detail, int8_t detail2);
void armarMsj(message_t *msj, char name, int8_t type, int8_t detail, int8_t detail2);
//Para el orquestador
orq_t armarOrqMsj_return(char *name, int8_t type, int8_t detail, char *ip, int port);
void armarOrqMsj(orq_t *msj, char *name, int8_t type, int8_t detail, char *ip, int port);

#endif /* PROTOCOLO_H_ */
