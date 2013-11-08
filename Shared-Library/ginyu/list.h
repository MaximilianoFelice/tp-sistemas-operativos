#ifndef GINYULIST_H_
#define GINYULIST_H_

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "../commons/collections/list.h"


void list_add_new(t_list *lista, void * data, int tamanio);
void list_add_in_index_new(t_list *lista, void * data, int tamanio, int indice);
int list_add_new_with_return(t_list *lista, void * data, int tamanio);
void* list_get_data(t_list* self, int index);

#endif /* LIST_H_ */

