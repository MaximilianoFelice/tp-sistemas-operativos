/*
 * grasa_Cache.c
 *
 *  Created on: 09/12/2013
 *      Author: utnso
 */

#include "grasa.h"
#include "grasa_cache.h"

void name_cache_free(struct node_number_cache_t *cache){
	cache->node_number = 0;
	free(cache->path);
	cache->path = malloc(0);
}

void name_cache_create(struct node_number_cache_t *cache){
	cache->node_number = 0;
	cache->path = malloc(0);
}

void name_cache_delete(struct node_number_cache_t *cache){
	free(cache->path);
}

int name_cache_look(struct node_number_cache_t *cache, const char* path){
	if (cache->node_number != 0) if (strcmp(path, cache->path) == 0) return cache->node_number;
	return -1;
}

void cache_renew(struct node_number_cache_t *cache, const char* path, int node_number){
	cache->node_number = (node_number);
	free(cache->path);
	cache->path = malloc(strlen(path)+1);
	strcpy(cache->path, path);
}
