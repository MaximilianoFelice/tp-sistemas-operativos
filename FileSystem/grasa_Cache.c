/*
 * grasa_Cache.c
 *
 *  Created on: 09/12/2013
 *      Author: utnso
 */

#include "grasa.h"
#include "grasa_cache.h"

pthread_rwlock_t _Cache_Access;
pthread_rwlockattr_t _Cache_Attr;


void name_cache_create(struct node_number_cache_t *cache){
	// Crea el lock
	pthread_rwlockattr_init(&_Cache_Attr);
	pthread_rwlockattr_setpshared(&_Cache_Attr, PTHREAD_PROCESS_SHARED);
	pthread_rwlock_init(&_Cache_Access, &_Cache_Attr);

	cache->node_number = 0;
	cache->path = malloc(0);
}

void name_cache_delete(struct node_number_cache_t *cache){
	free(cache->path);
	pthread_rwlockattr_destroy(&_Cache_Attr);
	pthread_rwlock_destroy(&_Cache_Access);
}


int name_cache_look(struct node_number_cache_t *cache, const char* path){
	int ret = -1;
	pthread_rwlock_rdlock(&_Cache_Access);
	if (cache->node_number != 0) if (strcmp(path, cache->path) == 0) ret = cache->node_number;
	pthread_rwlock_unlock(&_Cache_Access);
	return ret;
}

void cache_renew(struct node_number_cache_t *cache, const char* path, int node_number){
	pthread_rwlock_wrlock(&_Cache_Access);
	cache->node_number = (node_number);
	free(cache->path);
	cache->path = malloc(strlen(path)+1);
	strcpy(cache->path, path);
	pthread_rwlock_unlock(&_Cache_Access);
}

void name_cache_free(struct node_number_cache_t *cache){
	pthread_rwlock_wrlock(&_Cache_Access);
	cache->node_number = 0;
	free(cache->path);
	cache->path = malloc(0);
	pthread_rwlock_unlock(&_Cache_Access);
}
