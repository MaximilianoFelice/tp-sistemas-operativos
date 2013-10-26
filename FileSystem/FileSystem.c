/*
 * FileSystem.c
 *
 *  Created on: 28/09/2013
 *      Author: utnso
 */
#include "grasa.h"
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stddef.h>
#include <fuse.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/mman.h>
#include "bitarray.h"
#include <limits.h>		// Aqui obtenemos el CHAR_BIT, que nos permite obtener la cantidad de bits en un char.
//#include <commons/string.h>

/*
 * Este es el path de nuestro, relativo al punto de montaje, archivo dentro del FS
 */
#define DEFAULT_FILE_PATH "/" DEFAULT_FILE_NAME
#define CUSTOM_FUSE_OPT_KEY(t, p, v) { t, offsetof(struct t_runtime_options, p), v }

struct t_runtime_options {
	char* welcome_msg;
} runtime_options;


int lastchar(const char* str, char chr){
	if ( ( str[strlen(str)-1]  == chr) ) return 1;
	return 0;
}


/* @DESC
 * 		Determina cual es el nodo sobre el cual se encuentra un path.
 *
 * 	@PARAM
 * 		path - Direccion del directorio o archivo a buscar.
 *
 * 	@RETURN
 * 		Devuelve el numero de bloque en el que se encuentra el nombre.
 * 		Si el nombre no se encuentra, devuelve -1.
 *
 */
ptrGBloque determinar_nodo(const char* path){

	// Si es el directorio raiz, devuelve 0:
	if(!strcmp(path, "/")) return 0;

	int fd, i, nodo_anterior, aux;
	// Super_path usado para obtener la parte superior del path, sin el nombre.
	char *super_path = (char*) malloc(strlen(path) +1), *nombre = (char*) malloc(strlen(path)+1);
	char *start = nombre, *start_super_path = super_path; //Estos liberaran memoria.
	struct grasa_file_t *node, *inicio;
	unsigned char *node_name;
	strcpy(super_path, path);
	strcpy(nombre, path);
	// Obtiene y acomoda el nombre del archivo.
	if (lastchar(path, '/')) {
		nombre[strlen(nombre)-1] = '\0';
	}
	nombre = strrchr(nombre, '/');
	nombre[0] = '\0';
	nombre = &nombre[1]; // Acomoda el nombre, ya que el primer digito siempre es '/'

	// Acomoda el super_path
	if (lastchar(super_path, '/')) {
		super_path[strlen(super_path)-1] = '\0';
	}
	aux = strlen(super_path) - strlen(nombre);
	super_path[aux] = '\0';

	nodo_anterior = determinar_nodo(super_path);


	// Abrir conexion y traer directorios, guarda el bloque de inicio para luego liberar memoria
	if ((fd = open(DISC_PATH, O_RDONLY, 0)) == -1) {
		printf("ERROR");
		return -ENOENT;
	}
	node = (void*) mmap(NULL, HEADER_SIZE_B + BITMAP_SIZE_B + NODE_TABLE_SIZE_B , PROT_READ, MAP_SHARED, fd, 0);
	inicio = node;
	node = &(node[GFILEBYBLOCK + BITMAP_BLOCK_SIZE]);

	// Busca el nodo sobre el cual se encuentre el nombre.
	node_name = &(node->fname[0]);
	for (i = 0; ( (node->parent_dir_block != nodo_anterior) | (strcmp(nombre, (char*) node_name) != 0) | (node->state == 0)) &  (i < GFILEBYTABLE) ; i++ ){
		node = &(node[1]);
		node_name = &(node->fname[0]);
	}

	// Cierra conexiones y libera memoria.
	free(start);
	free(start_super_path);
	if (munmap(inicio, HEADER_SIZE_B + BITMAP_SIZE_B + NODE_TABLE_SIZE_B) == -1) printf("ERROR");
	close(fd);
	if (i >= GFILEBYTABLE) return -1;
	return (i+1);

}

/*
 * @DESC
 *  Esta función va a ser llamada cuando a la biblioteca de FUSE le llege un pedido
 * para obtener la metadata de un archivo/directorio. Esto puede ser tamaño, tipo,
 * permisos, dueño, etc ...
 *
 * @PARAMETROS
 * 		path - El path es relativo al punto de montaje y es la forma mediante la cual debemos
 * 		       encontrar el archivo o directorio que nos solicitan
 * 		stbuf - Esta esta estructura es la que debemos completar
 *
 * 	@RETURN
 * 		O archivo/directorio fue encontrado. -ENOENT archivo/directorio no encontrado
 *
 * 	@PERMISOS
 * 		Si es un directorio debe tener los permisos:
 * 			stbuf->st_mode = S_IFDIR | 0755;
 * 			stbuf->st_nlink = 2;
 * 		Si es un archivo:
 * 			stbuf->st_mode = S_IFREG | 0444;
 * 			stbuf->st_nlink = 1;
 * 			stbuf->st_size = [TAMANIO];
 *
 */
static int grasa_getattr(const char *path, struct stat *stbuf) {

	int nodo = determinar_nodo(path), fd;
	if (nodo < 0) return -ENOENT;
	struct grasa_file_t *node, *inicio;

	memset(stbuf, 0, sizeof(struct stat));

	if (strcmp(path, "/") == 0){
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
		return 0;
	}

	// Abrir conexion y traer directorios, guarda el bloque de inicio para luego liberar memoria
	if ((fd = open(DISC_PATH, O_RDONLY, 0)) == -1) {
		printf("ERROR");
		return -ENOENT;
	}
	node = (void*) mmap(NULL, HEADER_SIZE_B + BITMAP_SIZE_B + NODE_TABLE_SIZE_B , PROT_READ, MAP_SHARED, fd, 0);
	inicio = node;
	node = &(node[GFILEBYBLOCK + BITMAP_BLOCK_SIZE]);

	node = &(node[nodo-1]);

	if (node->state == 2){
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
		if (munmap(inicio, HEADER_SIZE_B + BITMAP_SIZE_B + NODE_TABLE_SIZE_B) == -1) printf("ERROR");
		return 0;
	} else if(node->state == 1){
		stbuf->st_mode = S_IFREG | 0444;
		stbuf->st_nlink = 1;
		stbuf->st_size = node->file_size;
		if (munmap(inicio, HEADER_SIZE_B + BITMAP_SIZE_B + NODE_TABLE_SIZE_B) == -1) printf("ERROR");
		return 0;
	}

	// Cierra conexiones y libera memoria.

	close(fd);
	if (munmap(inicio, HEADER_SIZE_B + BITMAP_SIZE_B + NODE_TABLE_SIZE_B) == -1) printf("ERROR");
	return -ENOENT;
}

/*
 * @DESC
 *  Esta función va a ser llamada cuando a la biblioteca de FUSE le llege un pedido
 * para obtener la lista de archivos o directorios que se encuentra dentro de un directorio
 *
 * @PARAMETROS
 * 		path - El path es relativo al punto de montaje y es la forma mediante la cual debemos
 * 		       encontrar el archivo o directorio que nos solicitan
 * 		buf - Este es un buffer donde se colocaran los nombres de los archivos y directorios
 * 		      que esten dentro del directorio indicado por el path
 * 		filler - Este es un puntero a una función, la cual sabe como guardar una cadena dentro
 * 		         del campo buf
 *
 * 	@RETURN
 * 		O directorio fue encontrado. -ENOENT directorio no encontrado
 */
static int grasa_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {
	int fd, i, nodo = determinar_nodo((char*) path);
	struct grasa_file_t *node, *inicio;


	if (nodo == -1) return -ENOENT;

	if ((fd = open(DISC_PATH, O_RDONLY, 0)) == -1) printf("ERROR");
	node = (void*) mmap(NULL, HEADER_SIZE_B + BITMAP_SIZE_B + NODE_TABLE_SIZE_B , PROT_READ, MAP_SHARED, fd, 0);
	inicio = node;
	node = &(node[GFILEBYBLOCK + BITMAP_BLOCK_SIZE]);

	// "." y ".." obligatorios.
	filler(buf, ".", NULL, 0);
	filler(buf, "..", NULL, 0);

	// Carga los nodos que cumple la condicion en el buffer.
	for (i = 0; i < GFILEBYTABLE;  (i++)){
		if ((nodo==(node->parent_dir_block)) & (((node->state) == 1) | ((node->state) == 2)))  filler(buf, (char*) &(node->fname[0]), NULL, 0);
		node = &node[1];
	}

	if (munmap(inicio, HEADER_SIZE_B + BITMAP_SIZE_B + NODE_TABLE_SIZE_B) == -1) printf("ERROR");
	close(fd);

	return 0;
}

/*
 * @DESC
 *  Esta función va a ser llamada cuando a la biblioteca de FUSE le llege un pedido
 * para tratar de abrir un archivo
 *
 * @PARAMETROS
 * 		path - El path es relativo al punto de montaje y es la forma mediante la cual debemos
 * 		       encontrar el archivo o directorio que nos solicitan
 * 		fi - es una estructura que contiene la metadata del archivo indicado en el path
 *
 * 	@RETURN
 * 		O archivo fue encontrado. -EACCES archivo no es accesible
 */
static int grasa_open(const char *path, struct fuse_file_info *fi) {

	return 0;
}

/*
 * @DESC
 *  Esta función va a ser llamada cuando a la biblioteca de FUSE le llege un pedido
 * para obtener el contenido de un archivo
 *
 * @PARAMETROS
 * 		path - El path es relativo al punto de montaje y es la forma mediante la cual debemos
 * 		       encontrar el archivo o directorio que nos solicitan
 * 		buf - Este es el buffer donde se va a guardar el contenido solicitado
 * 		size - Nos indica cuanto tenemos que leer
 * 		offset - A partir de que posicion del archivo tenemos que leer
 *
 * 	@RETURN
 * 		Si se usa el parametro direct_io los valores de retorno son 0 si  elarchivo fue encontrado
 * 		o -ENOENT si ocurrio un error. Si el parametro direct_io no esta presente se retorna
 * 		la cantidad de bytes leidos o -ENOENT si ocurrio un error. ( Este comportamiento es igual
 * 		para la funcion write )
 */
static int grasa_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
	(void) fi;
	int fd, nodo = determinar_nodo(path), bloque_punteros, num_bloque_datos;
	int bloque_a_buscar; // Estructura auxiliar para no dejar choclos
	struct grasa_file_t *node, *inicio, *inicio_data_block;
	ptrGBloque *pointer_block;
	char *data_block;
	size_t tam = size;

	// Abrir conexion y traer directorios, guarda el bloque de inicio para luego liberar memoria
	if ((fd = open(DISC_PATH, O_RDWR, 0)) == -1) printf("ERROR");
	node = (void*) mmap(NULL, ACTUAL_DISC_SIZE_B , PROT_WRITE | PROT_READ | PROT_EXEC, MAP_SHARED, fd, (GFILEBYBLOCK + BITMAP_BLOCK_SIZE)*BLOCKSIZE);
	inicio = node;
	inicio_data_block = &(node[NODE_TABLE_SIZE]);

	// Ubica el nodo correspondiente al archivo
	node = &(node[nodo-1]);

	// Recorre todos los punteros en el bloque de la tabla de nodos
	for (bloque_punteros = 0; bloque_punteros < BLKINDIRECT; bloque_punteros++){

		// Chequea el offset y lo acomoda para leer lo que realmente necesita
		if (offset > BLOCKSIZE * 1024){
			offset -= (BLOCKSIZE * 1024);
			continue;
		}

		bloque_a_buscar = (node->blk_indirect)[bloque_punteros];	// Ubica el nodo de punteros a nodos de datos, es relativo al nodo 0: Header.
		bloque_a_buscar -= (GFILEBYBLOCK + BITMAP_BLOCK_SIZE + NODE_TABLE_SIZE);	// Acomoda el nodo de punteros a nodos de datos, es relativo al bloque de datos.
		pointer_block =(ptrGBloque *) &(inicio_data_block[bloque_a_buscar]);		// Apunta al nodo antes ubicado. Lo utiliza para saber de donde leer los datos.

		// Recorre el bloque de punteros correspondiente.
		for (num_bloque_datos = 0; num_bloque_datos < 1024; num_bloque_datos++){

			// Chequea el offset y lo acomoda para leer lo que realmente necesita
			if (offset >= BLOCKSIZE){
				offset -= BLOCKSIZE;
				continue;
			}

			bloque_a_buscar = pointer_block[num_bloque_datos]; 	// Ubica el nodo de datos correspondiente. Relativo al nodo 0: Header.
			bloque_a_buscar -= (GFILEBYBLOCK + BITMAP_BLOCK_SIZE + NODE_TABLE_SIZE);	// Acomoda el nodo, haciendolo relativo al bloque de datos.
			data_block = (char *) &(inicio_data_block[bloque_a_buscar]);

			// Corre el offset hasta donde sea necesario para poder leer lo que quiere.
			if (offset > 0){
				data_block += offset;
				offset = 0;
			}

			if (tam < BLOCKSIZE){
				memcpy(buf, data_block, tam);
				buf = &(buf[tam]);
				tam = 0;
				break;
			} else {
				memcpy(buf, data_block, BLOCKSIZE);
				tam -= BLOCKSIZE;
				buf = &(buf[BLOCKSIZE]);
				if (tam == 0) break;
			}

		}

		if (tam == 0) break;
	}


	if (munmap(inicio, ACTUAL_DISC_SIZE_B ) == -1) printf("ERROR");

	close(fd);

	return size;


}

/*
 *  @ DESC
 * 		Esta estructura creara carpetas en el filesystem.
 * 		Notese que no debe nodificarse el Bitmap, ya que todas las estructuras administrativas quedan marcadas.
 *
 * 	@ PARAM
 *
 * 		-path: El path del directorio a crear
 *
 * 		-mode: Contiene los permisos que debe tener el directorio y otra metadata
 *
 * 	@ RET
 * 		Seguramente 0 si esta todo ok, negativo si hay error.
 */
int grasa_mkdir (const char *path, mode_t mode){

	int fd, nodo_padre, i, res = 0;
	struct grasa_file_t *node, *inicio;
	char *nombre = malloc(sizeof(path) +1), *nom_to_free = nombre;
	char *dir_padre = malloc(sizeof(path) +1), *dir_to_free = dir_padre;

	// Obtiene el nombre del path:
	strcpy(nombre, path);
	if (lastchar(nombre, '/')){
		nombre[strlen(nombre) -1] = '\0';
	}
	nombre = strrchr(nombre, '/');
	nombre = &(nombre[1]);

	// Obtiene el directorio superior:
	strcpy(dir_padre, path);
	if (lastchar(dir_padre, '/')){
		dir_padre[strlen(dir_padre) -1] = '\0';
	}
	(strrchr(dir_padre, '/'))[1] = '\0'; 	// Borra el nombre del dir_padre.

	// Ubica el nodo correspondiente. Si es el raiz, lo marca como 0, Si es menor a 0, lo crea (mismos permisos).
	if (strcmp(dir_padre, "/") == 0){
		nodo_padre = 0;
	} else if ((nodo_padre = determinar_nodo(dir_padre)) < 0){
		grasa_mkdir(path, mode);
	}

	// Abrir conexion y traer directorios, guarda el bloque de inicio para luego liberar memoria
	if ((fd = open(DISC_PATH, O_RDWR, 0)) == -1) printf("ERROR");
	node = (void*) mmap(NULL, HEADER_SIZE_B + BITMAP_SIZE_B + NODE_TABLE_SIZE_B , PROT_WRITE | PROT_READ | PROT_EXEC, MAP_SHARED, fd, (GFILEBYBLOCK)*BLOCKSIZE);
	inicio = node;

	// Busca el primer nodo libre (state 0) y cuando lo encuentra, lo crea:
	for (i = 0; (node->state != 0) & (i < NODE_TABLE_SIZE); i++) node = &(node[1]);
	// Si no hay un nodo libre, devuelve un error.
	if (i >= NODE_TABLE_SIZE){
		res = -ENOENT;
		goto finalizar;
	}

	// Escribe datos del archivo
	node->state = DIRECTORY_T;
	strcpy((char*) &(node->fname[0]), nombre);
	node->file_size = 0;
	node->parent_dir_block = nodo_padre;
	res = 0;

	finalizar:
	free(nom_to_free);
	free(dir_to_free);

	if (munmap(inicio, BITMAP_SIZE_B + NODE_TABLE_SIZE_B ) == -1) printf("ERROR");

	close(fd);

	return res;

}

/*
 *	@DESC
 *		Funcion que borra directorios de fuse.
 *
 *	@PARAM
 *		Path - El path donde tiene que borrar.
 *
 *	@RET
 *		0 Si esta OK, -ENOENT si no pudo.
 *
 *		Restaria que la funcion chequee si el dir esta vacio.
 *
 */
int grasa_rmdir (const char* path){

	int fd, nodo_padre = determinar_nodo(path);
	if (nodo_padre < 0) return -ENOENT;
	struct grasa_file_t *node, *inicio;

	// Abre conexiones y levanta la tabla de nodos en memoria.
	if ((fd = open(DISC_PATH, O_RDWR, 0)) == -1) printf("ERROR");
	node = (void*) mmap(NULL, HEADER_SIZE_B + BITMAP_SIZE_B + NODE_TABLE_SIZE_B , PROT_WRITE | PROT_READ | PROT_EXEC, MAP_SHARED, fd, (GFILEBYBLOCK)*BLOCKSIZE);
	inicio = node;

	node = &(node[nodo_padre]);

	node->state = DELETED_T; // Aca le dice que el estado queda "Borrado"

	// Cierra, ponele la alarma y se va para su casa. Mejor dicho, retorna 0 :D
	if (munmap(inicio, BITMAP_SIZE_B + NODE_TABLE_SIZE_B ) == -1) printf("ERROR");

	close(fd);

	return 0;
}

/*
 * 	@DESC
 * 		Trunca un archivo a un length correspondiente. El archivo puede perder datos
 *
 * 	@PARAM
 * 		path - La ruta del archivo.
 * 		new_size - El nuevo size.
 *
 * 	@RET
 * 		Como siempre, 0 si esta OK.
 *
 * 	FALTARIA QUE LA FUNCION RESERVE NODOS LIBRES.
 */
int grasa_truncate (const char *path, off_t new_size){
	int fd, nodo_padre = determinar_nodo(path);
	if (nodo_padre < 0) return -ENOENT;
	struct grasa_file_t *node, *inicio;

	// Abre conexiones y levanta la tabla de nodos en memoria.
	if ((fd = open(DISC_PATH, O_RDWR, 0)) == -1) printf("ERROR");
	node = (void*) mmap(NULL, HEADER_SIZE_B + BITMAP_SIZE_B + NODE_TABLE_SIZE_B , PROT_WRITE | PROT_READ | PROT_EXEC, MAP_SHARED, fd, (GFILEBYBLOCK)*BLOCKSIZE);
	inicio = node;

	node = &(node[nodo_padre]);

	node->file_size = new_size; // Aca le dice su nuevo size.

	// Cierra, ponele la alarma y se va para su casa. Mejor dicho, retorna 0 :D
	if (munmap(inicio, BITMAP_SIZE_B + NODE_TABLE_SIZE_B ) == -1) printf("ERROR");

	close(fd);

	return 0;
}

/*
 *  @DESC
 *  	Obtiene un bloque libre, actualiza el bitmap.
 *
 *  @PARAM
 *  	bitmap_start - Comienzo del Bitmap
 *  	bitmap_size - Tamanio del Bitmap
 *
 *  @RETURN
 *  	Devuelve el numero de un nodo libre listo para escribir. Si hay error, un numero negativo.
 */
int get_node(struct grasa_file_t *bitmap_start, size_t bitmap_size){
	t_bitarray *bitarray;
	int i, res;
	struct grasa_file_t *node = bitmap_start, *data_block = &node[BITMAP_BLOCK_SIZE + NODE_TABLE_SIZE];

	bitarray = bitarray_create((char*) bitmap_start, bitmap_size);

	// Encuentra el primer bit libre en la tabla de nodos.
	for (i = 0; (i <= bitmap_size) & (bitarray_test_bit(bitarray,i) == 1); i++);
	res = i;

	// Setea en 1 el bitmap.
	bitarray_set_bit(bitarray, i);

	// Limpia el contenido del nodo.
	i -= (GHEADERBLOCKS + BITMAP_BLOCK_SIZE + NODE_TABLE_SIZE);
	node = &data_block[i];
	memset(node, 0, BLOCKSIZE);

	// Cierra el bitmap
	bitarray_destroy(bitarray);

	return res;
}

/*
 *  @DESC
 *  	Actualiza la informacion del archivo.
 *
 *  @PARAM
 *
 *  @RET
 *  	Devuelve 0 si salio bien, negativo si hubo problemas.
 */
int add_node(struct grasa_file_t *file_data, struct grasa_file_t *inicio_data_block, int node_number, struct grasa_file_t *bitmap_start){
	int node_pointer_number, position;
	size_t tam = file_data->file_size;
	int new_pointer_block;
	size_t bitmap_size = (BITMAP_SIZE_B * CHAR_BIT);
	struct grasa_file_t *node = inicio_data_block;
	ptrGBloque *nodo_punteros;

	// Ubica nodo y posicion en el mismo.
	for (node_pointer_number = 0; (tam >= (BLOCKSIZE * 1024)); tam -= (BLOCKSIZE * 1024), node_pointer_number++);
	for (position = 0; (tam >= BLOCKSIZE); tam -= BLOCKSIZE, position++);

	// Si es el ultimo nodo en el bloque de punteros, pasa al siguiente
	if (position == 0){
		new_pointer_block = get_node(bitmap_start, bitmap_size);
		file_data->blk_indirect[node_pointer_number] = new_pointer_block;

	} else {
		new_pointer_block = file_data->blk_indirect[node_pointer_number]; //Se usa como auxiliar para encontrar el numero del bloque de punteros
	}

	// Ubica el nodo de punteros en *node, relativo al bloque de datos.
	nodo_punteros = (ptrGBloque*) &node[new_pointer_block - (GHEADERBLOCKS + NODE_TABLE_SIZE + BITMAP_BLOCK_SIZE)];

	// Hace que dicho puntero, en la posicion ya obtenida, apunte al nodo indicado.
	nodo_punteros[position] = node_number;

	return 0;

}

/*
 * NO USA EL OFFSET
 */
int set_position (int *pointer_block, int *data_block, size_t size, off_t offset){
	div_t divi;
	divi = div(offset, (BLOCKSIZE*1024));
	*pointer_block = divi.quot;
	*data_block = divi.rem / BLOCKSIZE;
	return 0;
}

/*
 * 	@DESC
 * 		Funcion que escribe archivos en fuse. Tiene la posta.
 *
 * 	@PARAM
 * 		path - Dir del archivo
 * 		buf - Buffer que indica que datos copiar.
 * 		size - Tam de los datos a copiar
 * 		offset - Situa una posicion sobre la cual empezar a copiar datos
 * 		fi - File Info. Contiene flags y otras cosas locas que no hay que usar
 *
 * 	@RET
 * 		Devuelve la cantidad de bytes escritos, siempre y cuando este OK. Caso contrario, numero negativo tipo -ENOENT.
 */
int grasa_write (const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi){
	(void) fi;
	int fd, nodo = determinar_nodo(path);
	int new_free_node;
	size_t bitmap_size = (BITMAP_SIZE_B * CHAR_BIT);
	struct grasa_file_t *node, *inicio_bitmap, *inicio_data_block;
	char *data_block;
	size_t tam = size, file_size;
	off_t off = offset;
	int *n_pointer_block = malloc(sizeof(int)), *n_data_block = malloc(sizeof(int));
	ptrGBloque *pointer_block;

	// Abrir conexion y traer directorios, guarda el bloque de inicio para luego liberar memoria
	if ((fd = open(DISC_PATH, O_RDWR, 0)) == -1) printf("ERROR");
	inicio_bitmap = (void*) mmap(NULL, ACTUAL_DISC_SIZE_B , PROT_WRITE | PROT_READ | PROT_EXEC, MAP_SHARED, fd, (GFILEBYBLOCK)*BLOCKSIZE);
	node = inicio_bitmap;
	node = (&node[BITMAP_BLOCK_SIZE]);
	inicio_data_block = &(node[NODE_TABLE_SIZE]);

	// Ubica el nodo correspondiente al archivo
	node = &(node[nodo-1]);
	file_size = node->file_size;


	// Guarda tantas veces como sea necesario, consigue nodos y actualiza el archivo.
	while (tam != 0){

		// Ubica a que nodo le corresponderia guardar el dato
		set_position(n_pointer_block, n_data_block, file_size, off);

		// Si el offset es mayor que el tamanio del archivo, significa que hay que pedir un bloque nuevo
		if ((off >= file_size) & (file_size != 0)){

			// Obtiene un bloque libre para escribir.
			new_free_node = get_node(inicio_bitmap, bitmap_size);

			// Actualiza la informacion del archivo.
			add_node(node, inicio_data_block, new_free_node, inicio_bitmap);

			// Lo relativiza al data block.
			new_free_node -= (GHEADERBLOCKS + NODE_TABLE_SIZE + BITMAP_BLOCK_SIZE);
			data_block = (char*) &(inicio_data_block[new_free_node]);

		} else {
			//Ubica el nodo a escribir.
			*n_pointer_block = node->blk_indirect[*n_pointer_block];
			*n_pointer_block -= (GHEADERBLOCKS + NODE_TABLE_SIZE + BITMAP_BLOCK_SIZE);
			pointer_block = (ptrGBloque*) &(inicio_data_block[*n_pointer_block]);
			*n_data_block = pointer_block[*n_data_block];
			*n_data_block -= (GHEADERBLOCKS + NODE_TABLE_SIZE + BITMAP_BLOCK_SIZE);
			data_block = (char*) &(inicio_data_block[*n_data_block]);
		}

		// Escribe en ese bloque de datos.
		if (tam >= BLOCKSIZE){
			memcpy(data_block, buf, BLOCKSIZE);
			off += BLOCKSIZE;
			tam -= BLOCKSIZE;
			file_size = node->file_size += BLOCKSIZE;
		} else {
			memcpy(data_block, buf, tam);
			file_size = node->file_size += tam;
			tam = 0;
		}

	}

	// Cierra lo que tiene en memoria.
	if (munmap(inicio_bitmap, ACTUAL_DISC_SIZE_B ) == -1) printf("ERROR");

	close(fd);

	return size;

}

/*
 *  @DESC
 *  	Se invoca esta funcion cada vez que fuse quiere hacer un archivo nuevo
 *
 *  @PARAM
 *  	path - Como siempre, el path del archivo relativo al disco
 *  	mode - Opciones del archivo
 *  	dev - Otra cosa que no se usa :D
 *
 *  @RET
 *  	Devuelve 0 si le sale OK, num negativo si no.
 */
int grasa_mknod (const char* path, mode_t mode, dev_t dev){
	int fd, nodo_padre, i, res = 0;
	int new_free_node;
	struct grasa_file_t *node, *inicio, *inicio_data_block;
	char *nombre = malloc(sizeof(path) + 1), *nom_to_free = nombre;
	char *dir_padre = malloc(sizeof(path) + 1), *dir_to_free = dir_padre;
	char *data_block;
	size_t bitmap_size = (BITMAP_SIZE_B * CHAR_BIT);

	// Obtiene el nombre del path:
	strcpy(nombre, path);
	if (lastchar(nombre, '/')){
		nombre[strlen(nombre) -1] = '\0';
	}
	nombre = strrchr(nombre, '/');
	nombre = &(nombre[1]);

	// Obtiene el directorio superior:
	strcpy(dir_padre, path);
	if (lastchar(dir_padre, '/')){
		dir_padre[strlen(dir_padre) -1] = '\0';
	}
	(strrchr(dir_padre, '/'))[1] = '\0'; 	// Borra el nombre del dir_padre.

	// Ubica el nodo correspondiente. Si es el raiz, lo marca como 0, Si es menor a 0, lo crea (mismos permisos).
	if (strcmp(dir_padre, "/") == 0){
		nodo_padre = 0;
	} else if ((nodo_padre = determinar_nodo(dir_padre)) < 0){
		grasa_mkdir(path, mode);
	}

	// Abrir conexion y traer directorios, guarda el bloque de inicio para luego liberar memoria
	if ((fd = open(DISC_PATH, O_RDWR, 0)) == -1) printf("ERROR");
	node = (void*) mmap(NULL, ACTUAL_DISC_SIZE_B , PROT_WRITE | PROT_READ | PROT_EXEC, MAP_SHARED, fd, (GFILEBYBLOCK)*BLOCKSIZE);
	inicio = node;

	// Busca el primer nodo libre (state 0) y cuando lo encuentra, lo crea:
	for (i = 0; (node->state != 0) & (i < NODE_TABLE_SIZE); i++) node = &(node[1]);
	// Si no hay un nodo libre, devuelve un error.
	if (i >= NODE_TABLE_SIZE){
		res = -ENOENT;
		goto finalizar;
	}

	// Escribe datos del archivo
	node->state = FILE_T;
	strcpy((char*) &(node->fname[0]), nombre);
	node->file_size = 0; // El tamanio se ira sumando a medida que se le reserven nodos.
	node->parent_dir_block = nodo_padre;
	res = 0;

	// Obtiene un bloque libre para escribir.
	new_free_node = get_node(inicio, bitmap_size);


	// Actualiza la informacion del archivo.
	inicio_data_block = &inicio[BITMAP_BLOCK_SIZE + NODE_TABLE_SIZE];
	add_node(node, inicio_data_block, new_free_node, inicio);

	// Lo relativiza al data block.
	new_free_node -= (GHEADERBLOCKS + NODE_TABLE_SIZE + BITMAP_BLOCK_SIZE);
	data_block = (char*) &(inicio_data_block[new_free_node]);

	// Escribe en ese bloque de datos.
	memset(data_block, '\0', BLOCKSIZE);

	finalizar:
	free(nom_to_free);
	free(dir_to_free);

	if (munmap(inicio, ACTUAL_DISC_SIZE_B) == -1) printf("ERROR");

	close(fd);

	return res;
}

/*
 *  @DESC
 *  	Funcion que se llama cuando hay que borrar un archivo
 *
 *  @PARAM
 *  	path - La ruta del archivo a borrar.
 *
 *  @RET
 *  	0 si salio bien
 *  	Numero negativo, si no
 */
int grasa_unlink (const char* path){
		return grasa_rmdir(path);
	}

/*
 * Esta es la estructura principal de FUSE con la cual nosotros le decimos a
 * biblioteca que funciones tiene que invocar segun que se le pida a FUSE.
 * Como se observa la estructura contiene punteros a funciones.
 */
static struct fuse_operations grasa_oper = {
		.readdir = grasa_readdir,	//OK
		.getattr = grasa_getattr,	//OK
		.open = grasa_open,			// OK
		.read = grasa_read,			// OK
		.mkdir = grasa_mkdir,		// OK - CHEQUEO DE NOMBRES IGUALES (VERIFICAR) // PROBLEMAS CON NOMBRES LARGOS
		.rmdir = grasa_rmdir,		// Queda implementar chequeo de directorio vacio.
		.truncate = grasa_truncate, // Queda implementar que la funcion reserve nodos y los libere.
		.write = grasa_write,		// Falta testearlo.
		.mknod = grasa_mknod,		// MISMO PROBLEMA CON NOMBRES LARGOS
		.unlink = grasa_unlink,		// Queda implementar la liberacion de los nodos que tenia.
};

/** keys for FUSE_OPT_ options */
enum {
	KEY_VERSION,
	KEY_HELP,
};

/* Carga los datos del header */
void Load_Header_Data(){
	int fd;
	struct grasa_header_t *node;

	if ((fd = open(DISC_PATH, O_RDWR, 0)) == -1) printf("ERROR");

	node = OPEN_HEADER(fd);

	Header_Data = *node;

}


/*
 * Esta estructura es utilizada para decirle a la biblioteca de FUSE que
 * parametro puede recibir y donde tiene que guardar el valor de estos
 */
static struct fuse_opt fuse_options[] = {
		// Este es un parametro definido por nosotros
		CUSTOM_FUSE_OPT_KEY("--welcome-msg %s", welcome_msg, 0),

		// Estos son parametros por defecto que ya tiene FUSE
		FUSE_OPT_KEY("-V", KEY_VERSION),
		FUSE_OPT_KEY("--version", KEY_VERSION),
		FUSE_OPT_KEY("-h", KEY_HELP),
		FUSE_OPT_KEY("--help", KEY_HELP),
		FUSE_OPT_END,
};


// Dentro de los argumentos que recibe nuestro programa obligatoriamente
// debe estar el path al directorio donde vamos a montar nuestro FS
fuse_fill_dir_t* functi_filler(void *buf, const char *name,const struct stat *stbuf, off_t off){
	printf("Recibi carpeta: %s\n", name);
	return 0;
}


int main (int argc, char *argv[]){

	Load_Header_Data();


	//---------- LETS TRY DOWN HERE!!! -----------

//	char *CTMAB = malloc(100);
//	CTMAB[50] = '\0';
//	strcpy(CTMAB, "LA CONCHA DE TU MADRE ALL BOYS");
//	CTMAB = &(CTMAB[5]);
//	CTMAB[strlen(CTMAB)-10] = '\0';
//	printf("%d", strlen(CTMAB));
//	free(&(CTMAB[-5]));

//	escribir_uno();


//
//	char *buf = malloc(16384);
//	size_t size = 16384;
//	off_t offset = 0;
//	struct fuse_file_info *fi = ((struct fuse_file_info *) NULL);
//
//	grasa_read("/dir1/secret/top_secret.jpg", buf, size, offset, fi);
//
//	int fd;
//	fd = open("imagen.jpg", O_RDWR, 0);
//	printf("%s", buf);
//	write(fd, buf, size);
//	close(fd);
//	free(buf);


//	mode_t elModo;
//	elModo = 0;
//	printf("%d", grasa_mkdir("/carlos/", elModo));
//	 return 0

//
//	mode_t mode;
//	dev_t dev;
//	grasa_mknod("/holaa.txt", mode, dev);
//	grasa_mkdir("/yosoydeindependiente", mode);
//	return 0;
	//---------- LETS END OUR TRIAL =( -----------








	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);

	// Limpio la estructura que va a contener los parametros
	memset(&runtime_options, 0, sizeof(struct t_runtime_options));

	// Esta funcion de FUSE lee los parametros recibidos y los intepreta
	if (fuse_opt_parse(&args, &runtime_options, fuse_options, NULL) == -1){
		/** error parsing options */
		perror("Invalid arguments!");
		return EXIT_FAILURE;
	}

	// Si se paso el parametro --welcome-msg
	// el campo welcome_msg deberia tener el
	// valor pasado
	if( runtime_options.welcome_msg != NULL ){
		printf("%s\n", runtime_options.welcome_msg);
	}

	// Esta es la funcion principal de FUSE, es la que se encarga
	// de realizar el montaje, comuniscarse con el kernel, delegar
	// en varios threads
	return fuse_main(args.argc, args.argv, &grasa_oper, NULL);

}
