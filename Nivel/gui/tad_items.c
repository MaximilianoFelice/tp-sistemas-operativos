#include "tad_items.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

ITEM_NIVEL* _search_item_by_id(t_list* items, char id);

void CrearItem(t_list* items, char id, int x, int y, char tipo, int cant_rec) {
	ITEM_NIVEL * item = malloc(sizeof(ITEM_NIVEL));

	item->id = id;
	item->posx = x;
	item->posy = y;
	item->item_type = tipo;
	item->quantity = cant_rec;
	item->num_enemy=0; //Si es un enemigo es >0; si es un PJ muerto es -99

	list_add(items, item);
}

void CrearPersonaje(t_list* items, char id, int x, int y) {
	CrearItem(items, id, x, y, PERSONAJE_ITEM_TYPE, 0);
}

void CrearEnemigo(t_list* items, char id, int x, int y) {
	CrearItem(items, id, x, y, ENEMIGO_ITEM_TYPE, 0);
}

void CrearCaja(t_list* items, char id, int x, int y, int cant) {
	CrearItem(items, id, x, y, RECURSO_ITEM_TYPE, cant);
}

void BorrarItem(t_list* items, char id) {
	bool _search_by_id(ITEM_NIVEL* item) {
		return item->id == id;
	}

	list_remove_by_condition(items, (void*) _search_by_id);
}

void BorrarPersonaje(t_list* items, char id) {
	BorrarItem(items, id);
}

void MoverPersonaje(t_list* items, char id, int x, int y) {
	ITEM_NIVEL* item = _search_item_by_id(items, id);

	if (item != NULL ) {
		item->posx = x;
		item->posy = y;
	} else {
		printf("WARN: Item %c no existente\n", id);
	}
}

//NO USAR ESTA, USAR SIEMPRE MoveEnemy()
void MoverEnemigo(t_list* items, char id, int x, int y) {
	MoverPersonaje(items, id, x, y);
}

void restarRecurso(t_list* items, char id) {
	ITEM_NIVEL* item = _search_item_by_id(items, id);

	if (item != NULL ) {
		item->quantity = item->quantity > 0 ? item->quantity - 1 : 0;
	} else {
		printf("WARN: Item %c no existente\n", id);
	}
}

void sumarRecurso(t_list* items, char id) {

	ITEM_NIVEL* item = _search_item_by_id(items, id);

	if (item != NULL ) {
		item->quantity = item->quantity > 0 ? item->quantity + 1 : 0;
	} else {
		printf("WARN: Item %c no existente\n", id);
	}
}

ITEM_NIVEL* _search_item_by_id(t_list* items, char id) {
	bool _search_by_id(ITEM_NIVEL* item) {
		return item->id == id;
	}

	return list_find(items, (void*) _search_by_id);
}

//*****************************Begin funciones de enemigos*******************************************

ITEM_NIVEL* _search_enemy(t_list* items, int num_enemy) {

	bool _search_by_numEnemy_and_id(ITEM_NIVEL* item) {
		return item->num_enemy==num_enemy;
	}

	return list_find(items, (void*) _search_by_numEnemy_and_id);
}

void MoveEnemy(t_list* items, int id_enemy, int x, int y) {

	ITEM_NIVEL* item = _search_enemy(items, id_enemy);

	if (item != NULL ) {
		item->posx = x;
		item->posy = y;
	} else {
		printf("WARN: Item %c no existente\n", '*');
	}
}

void CreateEnemy(t_list* items, int id_enemy, int x, int y) {

	ITEM_NIVEL * item = malloc(sizeof(ITEM_NIVEL));

	item->id = '*';
	item->posx = x;
	item->posy = y;
	item->item_type = ENEMIGO_ITEM_TYPE;
	item->quantity = 0;
	item->num_enemy = id_enemy;

	list_add(items, item);

}

void DeleteEnemy(t_list* items, int id_enemy) {
	bool _search_by_id_enemy(ITEM_NIVEL* item) {
		return item->num_enemy == id_enemy;
	}

	list_remove_by_condition(items, (void*) _search_by_id_enemy);
}

void getPosEnemy(t_list * items, int id_enemy, int *x, int *y){

	ITEM_NIVEL *item;
	item = _search_enemy(items, id_enemy);
	*x = item->posx;
	*y = item->posy;

}

//*****************************Fin funciones de enemigos*******************************************


/*
 * @NAME: getPosRecurso
 * @DESC: busca un recurso por id y actualiza las variables posX y posY que recibe por parametro con esas posiciones
 */
void getPosRecurso(t_list * items, char id, int *posX, int *posY) {
	ITEM_NIVEL* item;
	item = _search_item_by_id(items, id);
	*posX = item->posx;
	*posY = item->posy;
}

void getPosPersonaje(t_list * items, char id, int *x, int *y) {
	ITEM_NIVEL *item;
	item = _search_item_by_id(items, id);
	*x = item->posx;
	*y = item->posy;
}


/*
 * @NAME: restarInstanciasRecurso()
 * @DESC:Resta y retorna la cantidad de instancias de un recurso. En caso de no encontrar el item devuelve -1
 *
 */
int restarInstanciasRecurso(t_list *items, char id) {

	ITEM_NIVEL* item = _search_item_by_id(items, id);

	if (item != NULL ) {
		if(item->quantity > 0){
			item->quantity--;
			return item->quantity;
		}
		else
			return -2;
	} else {
		printf("WARN: Item %c no existente\n", id);
		return -1;
	}
}

/*
 * @NAME: sumarInstanciasRecurso()
 * @DESC: Suma y retorna la cantidad de instancias de un recurso. En caso de no encontrar el item devuelve -1
 *
 */
int sumarInstanciasRecurso(t_list *items, char id) {

	ITEM_NIVEL* item = _search_item_by_id(items, id);

	if (item != NULL ) {
		item->quantity = item->quantity >= 0 ? item->quantity + 1 : 0;
		return item->quantity;
	} else {
		printf("WARN: Item %c no existente\n", id);
		return -1;
	}

}
