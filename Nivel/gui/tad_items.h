#ifndef __TAD_ITEMS__

#define __TAD_ITEMS__

#include "nivel.h"
#include <commons/collections/list.h>

void BorrarItem(t_list* items, char id);
void BorrarPersonaje(t_list* items, char id);
void DeleteEnemy(t_list* items, int id_enemy);

void restarRecurso(t_list* items, char id);
void sumarRecurso(t_list* items, char id);

void MoverPersonaje(t_list* items, char personaje, int x, int y);
void MoverEnemigo(t_list* items, char personaje, int x, int y);
void MoveEnemy(t_list* items, int id_enemy, int x, int y);

void CrearPersonaje(t_list* items, char id, int x , int y);
void CrearEnemigo(t_list* items, char id, int x , int y);
void CrearCaja(t_list* items, char id, int x , int y, int cant);
void CrearItem(t_list* items, char id, int x, int y, char tipo, int cant);
void CreateEnemy(t_list* items,int id_enemy, int x, int y);

void getPosRecurso(t_list * items, char id, int *posX, int *posY);
void getPosPersonaje(t_list * items, char id, int *x, int *y);
void getPosEnemy(t_list * items, int id_enemy, int *x, int *y);

int restarInstanciasRecurso(t_list *items, char id);
int sumarInstanciasRecurso(t_list *items, char id);


#endif

