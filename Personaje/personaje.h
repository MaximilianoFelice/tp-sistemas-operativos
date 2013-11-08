#ifndef PERSONAJE_H_
#define PERSONAJE_H_

#include <commons/collections/list.h>
#include <ginyu/config.h>
#include <ginyu/list.h>
#include <ginyu/log.h>
#include <ginyu/sockets.c>
#include <ginyu/protocolo.h>


#include <commons/collections/list.h>
#include <commons/config.h>
#include <commons/string.h>
#include <commons/log.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

typedef struct Nivel {
	char *nomNivel;
	t_list *Objetivos;
	int num_of_thread;
} nivel_t;

typedef struct ThreadNivel{
	pthread_t thread;
	nivel_t nivel;
} threadNivel_t;

void *jugar(void *argumentos);
int calculaMovimiento(int posX, int posY, int posRX, int posRY);
void handshake_orq(int *sockOrq, int *puertoPlanif, char*ip_planif, char *nom_nivel);
void handshake_planif(int *sockPlan, int *posX, int *posY);
static void nivel_destroyer(nivel_t* nivel);
void actualizaPosicion(int movimiento, int *posX, int *posY);
void morir(char* causaMuerte, int *currObj);
bool devolverRecursos(int *sockPlan, message_t *message);
bool validarSenial(bool *murioPersonaje, int *currObj);
void aumentarVidas();
void morirSenial();
void restarVidas();


#endif /* PERSONAJE_H_ */
