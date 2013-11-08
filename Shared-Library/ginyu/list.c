#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "../commons/collections/list.h"

#include "list.h"

/*
 * @NAME: list_add_new
 * @DESC: aloca memoria y agrega un elemento a la lista
 * @PARAMS: lista donde se agrega el elemento, la data que se va a agregar y el tamaño de la data
 */

void list_add_new(t_list *lista, void * data, int tamanio) {
	void* memoriaParaNodo = malloc(tamanio);
	memcpy(memoriaParaNodo, data, tamanio);
	list_add(lista, memoriaParaNodo);
}

/*
 * @NAME: list_add_new_in_index
 * @DESC: aloca memoria, agrega  un elemento a la lista en un lugar determinado
 * @PARAMS: lista donde se agrega el elemento, la data que se va a agregar y el tamaño de la data
 * 			y la pos del indice en la lista
 */

void list_add_in_index_new(t_list *lista, void * data, int tamanio, int indice){
	void* memoriaParaNuevoNodo = malloc(tamanio);
	memcpy(memoriaParaNuevoNodo, data, tamanio);
	list_add_in_index(lista, indice, memoriaParaNuevoNodo);
}

/*
 * @NAME: list_add_new_with_return
 * @DESC: aloca memoria, agrega un elemento a la lista y retorna la cantidad de elementos de la lista
 * @PARAMS: lista donde se agrega el elemento y la data que se va a agregar y
 */

int list_add_new_with_return(t_list *lista, void * data, int tamanio) {
	void* memoriaParaNodo = malloc(tamanio);
	memcpy(memoriaParaNodo, data, tamanio);
	return (list_add(lista, memoriaParaNodo));
}

/*
 * @NAME: list_try_get_data
 * @DESC: Retorna el contenido de un nodo en una posicion en la lista, o NULL si no lo encuentra o es invalido el indice
 */
void* list_get_data(t_list* self, int index) {
	int cont = 0;

	//Verifa que el indice sea valido, si no lo es devuelve NULL
	if ((self->elements_count > index) && (index >= 0)) {

		t_link_element *element = self->head;

		//Recorre la lista hasta la posicion del indice
		while (cont < index) {
			element = element->next;
			cont++;
		}
		//Si hay un elemento en esa posicion retorna el elemento, sinó devuelve NULL
		if (element != NULL)
			return element->data;
		else
			return NULL;
	}
	return NULL ;
}

