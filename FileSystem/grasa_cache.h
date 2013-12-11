/*
 * grasa_cache.h
 *
 *  Created on: 09/12/2013
 *      Author: utnso
 */

#ifndef GRASA_CACHE_H_
#define GRASA_CACHE_H_

/* Codigos de error para la cache */
#define CACHE_OVERFLOW 1

/* Variable que se utilizara para guardar un cache interno de un solo nodo, con algoritmo LRU */
struct node_number_cache_t {
	uint32_t node_number;
	char *path;
};
struct node_number_cache_t node_cache;

/* Declaracion de funciones */

void name_cache_free(struct node_number_cache_t *cache);
void name_cache_create(struct node_number_cache_t *cache);
void name_cache_delete(struct node_number_cache_t *cache);
int name_cache_look(struct node_number_cache_t *cache, const char* path);
void cache_renew(struct node_number_cache_t *cache, const char* path, int node_number);

#endif /* GRASA_CACHE_H_ */

