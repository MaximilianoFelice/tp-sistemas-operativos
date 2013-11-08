
#ifndef PROTOCOLO_H_
#define PROTOCOLO_H_


#include <stdint.h> //para los "int8_t"//////
#include <unistd.h> //para que no tire warning el close(i);

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

//Estado
#define BLOCK 10
#define OTORGADO 11
#define MUERTO_DEADLOCK 78
#define MUERTO_ENEMIGOS 77

//Movimientos
#define ARRIBA 12
#define DERECHA 13
#define ABAJO 14
#define IZQUIERDA 15

//Emisor
#define NIVEL 16
#define PERSONAJE 17

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

#endif /* PROTOCOLO_H_ */
