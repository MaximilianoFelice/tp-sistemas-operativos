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
#include <commons/bitarray.h>
#include <semaphore.h>
#include <pthread.h>
#include <commons/log.h>
#include <commons/string.h>
#include <signal.h>
#include <time.h>

// Definimos el semaforo que se utilizará para poder escribir:
pthread_rwlock_t rwlock = PTHREAD_RWLOCK_INITIALIZER;
t_log* logger;

/*
 * Este es el path de nuestro, relativo al punto de montaje, archivo dentro del FS
 */
#define DEFAULT_FILE_PATH "/" DEFAULT_FILE_NAME
#define CUSTOM_FUSE_OPT_KEY(t, p, v) { t, offsetof(struct t_runtime_options, p), v }

// Define los datos del log
#define LOG_PATH fuse_log_path
char fuse_log_path[1000];

struct t_runtime_options {
	char* welcome_msg;
	char* define_disc_path;
	char* log_level_param;
	char* log_path_param;
} runtime_options;

// Define los datos de mappeo de memoria:
struct grasa_header_t *header_start;
struct grasa_file_t *node_table_start, *data_block_start, *bitmap_start;

// Utiliza esta estructura para almacenar el numero de descriptor en el cual se abrio el disco
int discDescriptor;


/*
 * 	DESC
 * 		Divide el path con formato de [RUTA] en: [RUTA_SUPERIOR] y [NOMBRE].
 * 		Ejemplo:
 * 			path: /home/utnso/algo.txt == /home/utnso - algo.txt
 * 			path: /home/utnso/ == /home - utnso
 *
 * 	PARAM
 * 		path - Ruta a dividir
 * 		super_path - Puntero sobre el cual se guardara la ruta superior.
 * 		name - Puntero al nombre del archivo
 *
 * 	RET
 * 		0... SIEMPRE!
 *
 */
int split_path(const char* path, char** super_path, char** name){
	int aux;
	strcpy(*super_path, path);
	strcpy(*name, path);
	// Obtiene y acomoda el nombre del archivo.
	if (lastchar(path, '/')) {
		(*name)[strlen(*name)-1] = '\0';
	}
	*name = strrchr(*name, '/');
	*name = *name + 1; // Acomoda el nombre, ya que el primer digito siempre es '/'

	// Acomoda el super_path
	if (lastchar(*super_path, '/')) {
		(*super_path)[strlen(*super_path)-1] = '\0';
	}
	aux = strlen(*super_path) - strlen(*name);
	(*super_path)[aux] = '\0';

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

	int i, nodo_anterior, err = 0;
	// Super_path usado para obtener la parte superior del path, sin el nombre.
	char *super_path = (char*) malloc(strlen(path) +1), *nombre = (char*) malloc(strlen(path)+1);
	char *start = nombre, *start_super_path = super_path; //Estos liberaran memoria.
	struct grasa_file_t *node;
	unsigned char *node_name;

	split_path(path, &super_path, &nombre);

	nodo_anterior = determinar_nodo(super_path);


	pthread_rwlock_rdlock(&rwlock); //Toma un lock de lectura.
			log_lock_trace(logger, "Determinar_nodo: Toma lock lectura. Cantidad de lectores: %d", rwlock.__data.__nr_readers);

	node = node_table_start;

	// Busca el nodo sobre el cual se encuentre el nombre.
	node_name = &(node->fname[0]);
	for (i = 0; ( (node->parent_dir_block != nodo_anterior) | (strcmp(nombre, (char*) node_name) != 0) | (node->state == 0)) &  (i < GFILEBYTABLE) ; i++ ){
		node = &(node[1]);
		node_name = &(node->fname[0]);
	}

	// Cierra conexiones y libera memoria.
	pthread_rwlock_unlock(&rwlock);
			log_lock_trace(logger, "Determinar_nodo: Libera lock lectura. Cantidad de lectores: %d", rwlock.__data.__nr_readers);
	free(start);
	free(start_super_path);
	if (err != 0) return err;
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
			log_info(logger, "Getattr: Path: %s", path);
	int nodo = determinar_nodo(path), res;
	if (nodo < 0) return -ENOENT;
	struct grasa_file_t *node;
	memset(stbuf, 0, sizeof(struct stat));

	if (strcmp(path, "/") == 0){
		stbuf->st_mode = S_IFDIR | 0777;
		stbuf->st_nlink = 2;
		return 0;
	}

	pthread_rwlock_rdlock(&rwlock); //Toma un lock de lectura.
			log_lock_trace(logger, "Getattr: Toma lock lectura. Cantidad de lectores: %d", rwlock.__data.__nr_readers);

	node = node_table_start;

	node = &(node[nodo-1]);

	if (node->state == 2){
		stbuf->st_mode = S_IFDIR | 0777;
		stbuf->st_nlink = 2;
		stbuf->st_size = 4096; // Default para los directorios, es una "convencion".
		stbuf->st_mtime = node->m_date;
		stbuf->st_ctime = node->c_date;
		res = 0;
		goto finalizar;
	} else if(node->state == 1){
		stbuf->st_mode = S_IFREG | 0777;
		stbuf->st_nlink = 1;
		stbuf->st_size = node->file_size;
		stbuf->st_mtime = node->m_date;
		stbuf->st_ctime = node->c_date;
		res = 0;
		goto finalizar;
	}

	res = -ENOENT;
	// Cierra conexiones y libera memoria.
	finalizar:
	pthread_rwlock_unlock(&rwlock); // Libera el lock.
			log_lock_trace(logger, "Getattr:: Libera lock lectura. Cantidad de lectores: %d", rwlock.__data.__nr_readers);
	return res;
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
			log_info(logger, "Readdir: Path: %s - Offset %d", path, offset);
	int i, nodo = determinar_nodo((char*) path), res = 0;
	struct grasa_file_t *node;

	if (nodo == -1) return  -ENOENT;


	node = node_table_start;

	// "." y ".." obligatorios.
	filler(buf, ".", NULL, 0);
	filler(buf, "..", NULL, 0);

	pthread_rwlock_rdlock(&rwlock); //Toma un lock de lectura.
			log_lock_trace(logger, "Readdir: Toma lock lectura. Cantidad de lectores: %d", rwlock.__data.__nr_readers);


	// Carga los nodos que cumple la condicion en el buffer.
	for (i = 0; i < GFILEBYTABLE;  (i++)){
		if ((nodo==(node->parent_dir_block)) & (((node->state) == 1) | ((node->state) == 2)))  filler(buf, (char*) &(node->fname[0]), NULL, 0);
		node = &node[1];
	}


	pthread_rwlock_unlock(&rwlock); //Devuelve un lock de lectura.
			log_lock_trace(logger, "Readdir: Libera lock lectura. Cantidad de lectores: %d", rwlock.__data.__nr_readers);
	return res;
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
			log_info(logger, "Reading: Path: %s - Size: %d - Offset %d", path, size, offset);
	(void) fi;
	unsigned int nodo = determinar_nodo(path), bloque_punteros, num_bloque_datos;
	unsigned int bloque_a_buscar; // Estructura auxiliar para no dejar choclos
	struct grasa_file_t *node;
	ptrGBloque *pointer_block;
	char *data_block;
	size_t tam = size;
	int res;

	node = node_table_start;

	// Ubica el nodo correspondiente al archivo
	node = &(node[nodo-1]);

	pthread_rwlock_rdlock(&rwlock); //Toma un lock de lectura.
			log_lock_trace(logger, "Read: Toma lock lectura. Cantidad de lectores: %d", rwlock.__data.__nr_readers);

	if(node->file_size <= offset){
		log_error(logger, "Fuse intenta leer un offset mayor que el tamanio de archivo. Se retorna size 0.");
		res = 0;
		goto finalizar;
	}
	// Recorre todos los punteros en el bloque de la tabla de nodos
	for (bloque_punteros = 0; bloque_punteros < BLKINDIRECT; bloque_punteros++){

		// Chequea el offset y lo acomoda para leer lo que realmente necesita
		if (offset > BLOCKSIZE * 1024){
			offset -= (BLOCKSIZE * 1024);
			continue;
		}

		bloque_a_buscar = (node->blk_indirect)[bloque_punteros];	// Ubica el nodo de punteros a nodos de datos, es relativo al nodo 0: Header.
		bloque_a_buscar -= (GFILEBYBLOCK + BITMAP_BLOCK_SIZE + NODE_TABLE_SIZE);	// Acomoda el nodo de punteros a nodos de datos, es relativo al bloque de datos.
		pointer_block =(ptrGBloque *) &(data_block_start[bloque_a_buscar]);		// Apunta al nodo antes ubicado. Lo utiliza para saber de donde leer los datos.

		// Recorre el bloque de punteros correspondiente.
		for (num_bloque_datos = 0; num_bloque_datos < 1024; num_bloque_datos++){

			// Chequea el offset y lo acomoda para leer lo que realmente necesita
			if (offset >= BLOCKSIZE){
				offset -= BLOCKSIZE;
				continue;
			}

			bloque_a_buscar = pointer_block[num_bloque_datos]; 	// Ubica el nodo de datos correspondiente. Relativo al nodo 0: Header.
			bloque_a_buscar -= (GFILEBYBLOCK + BITMAP_BLOCK_SIZE + NODE_TABLE_SIZE);	// Acomoda el nodo, haciendolo relativo al bloque de datos.
			data_block = (char *) &(data_block_start[bloque_a_buscar]);

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
	res = size;

	finalizar:
	pthread_rwlock_unlock(&rwlock); //Devuelve el lock de lectura.
			log_lock_trace(logger, "Read: Libera lock lectura. Cantidad de lectores: %d", rwlock.__data.__nr_readers);

			log_trace(logger, "Terminada lectura.");
	return res;


}

/*
 *  @DESC
 *  	Obtiene un bloque libre, actualiza el bitmap.
 *
 *  @PARAM
*		(void)
 *
 *  @RETURN
 *  	Devuelve el numero de un nodo libre listo para escribir. Si hay error, un numero negativo.
 */
int get_node(void){
	t_bitarray *bitarray;
	int i, res;
	struct grasa_file_t *node = bitmap_start, *data_block = &node[BITMAP_BLOCK_SIZE + NODE_TABLE_SIZE];

	bitarray = bitarray_create((char*) bitmap_start, BITMAP_SIZE_B);

	// Encuentra el primer bit libre en la tabla de nodos.
	for (i = 0; (i <= BITMAP_SIZE_BITS) & (bitarray_test_bit(bitarray,i) == 1); i++);
	res = i;

	// Setea en 1 el bitmap.
	bitarray_set_bit(bitarray, i);
	bitmap_free_blocks--;

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
 *		file_data - El puntero al nodo en el que se encuentra el archivo.
 *		node_number - El numero de nodo que se le debe agregar.
 *
 *  @RET
 *  	Devuelve 0 si salio bien, negativo si hubo problemas.
 */
int add_node(struct grasa_file_t *file_data, int node_number){
	int node_pointer_number, position;
	size_t tam = file_data->file_size;
	int new_pointer_block;
	struct grasa_file_t *node = data_block_start;
	ptrGBloque *nodo_punteros;

	// Ubica el ultimo nodo escrito y se posiciona en el mismo.
	for (node_pointer_number = 0; (tam >= (BLOCKSIZE * 1024)); tam -= (BLOCKSIZE * 1024), node_pointer_number++);
	for (position = 0; (tam >= BLOCKSIZE); tam -= BLOCKSIZE, position++);

	// Si es el primer nodo del archivo y esta escrito, debe escribir el segundo.
	// Se sabe que el primer nodo del archivo esta escrito siempre que el primer puntero a bloque punteros del nodo sea distinto de 0 (file_data->blk_indirect[0] != 0)
	// ya que se le otorga esa marca (=0) al escribir el archivo, para indicar que es un archivo nuevo.
	if ((file_data->blk_indirect[node_pointer_number] != 0)){
		if (position == 1024) {
			position = 0;
			node_pointer_number++;
		}
	}
	// Si es el ultimo nodo en el bloque de punteros, pasa al siguiente
	if (position == 0){
		new_pointer_block = get_node();
		file_data->blk_indirect[node_pointer_number] = new_pointer_block;
		// Cuando crea un bloque, settea al siguente como 0, dejando una marca.
		file_data->blk_indirect[node_pointer_number +1] = 0;
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
 * 		Seguramente 0 si esta ok, negativo si hay error.
 */
int grasa_mkdir (const char *path, mode_t mode){
			log_info(logger, "Mkdir: Path: %s", path);
	int nodo_padre, i, res = 0;
	struct grasa_file_t *node;
	char *nombre = malloc(strlen(path) + 1), *nom_to_free = nombre;
	char *dir_padre = malloc(strlen(path) + 1), *dir_to_free = dir_padre;

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

	// Toma un lock de escritura.
			log_lock_trace(logger, "Mkdir: Pide lock escritura. Escribiendo: %d. En cola: %d.", rwlock.__data.__writer, rwlock.__data.__nr_writers_queued);
	pthread_rwlock_wrlock(&rwlock);
			log_lock_trace(logger, "Mkdir: Recibe lock escritura.");
	// Abrir conexion y traer directorios, guarda el bloque de inicio para luego liberar memoria
	node = bitmap_start;

	// Busca si existe algun otro directorio con ese nombre. Caso afirmativo, se lo avisa a FUSE con -EEXIST.
	for (i=0; i < 1024 ;i++){
		char *fname;
		fname = (char*) &((&node_table_start[i])->fname);
		if (((&node_table_start[i])->state != DELETED_T) & (strcmp(fname, nombre) == 0)) {
			res = -EEXIST;
			goto finalizar;
		}
	}

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

	// Devuelve el lock de escritura.
	pthread_rwlock_unlock(&rwlock);
			log_lock_trace(logger, "Mkdir: Devuelve lock escritura. En cola: %d", rwlock.__data.__nr_writers_queued);
	return res;

}

/*
 * 	@DESC
 * 		Setea la posicion del pointer_block y un data_block respecto a un archivo
 * 		Se utiliza para localizar donde escribir.
 *
 * 	@PARAM
 * 		pointer_block - puntero al bloque de punteros a settear
 * 		data_block - puntero al bloque de datos a settear
 * 		size - tamanio a escribir del archivo (no se usa, se incorpora pensando en debugging)
 * 		offset - corrimiento que se debe tener del principio del archivo
 *
 * 	@RET
 * 		Devuelve 0. No se contempla error.
 */
int set_position (int *pointer_block, int *data_block, size_t size, off_t offset){
	div_t divi;
	divi = div(offset, (BLOCKSIZE*1024));
	*pointer_block = divi.quot;
	*data_block = divi.rem / BLOCKSIZE;
	return 0;
}

/*
 *	@DESC
 *		Borra los nodos hasta la estructura correspondiente (upto:hasta) especificadas. EXCLUSIVE.
 *
 *	@PARAM
 *
 *
 *	@RET
 *
 */
int delete_nodes_upto (struct grasa_file_t *file_data, int pointer_upto, int data_upto){
	t_bitarray *bitarray;
	size_t file_size = file_data->file_size;
	int node_to_delete, node_pointer_to_delete;
	ptrGBloque *aux; // Auxiliar utilizado para saber que nodo redireccionar
	int data_pos, pointer_pos;

	// Ubica cual es el ultimo nodo del archivo
	set_position(&pointer_pos, &data_pos, 0, file_size);

	// Crea el bitmap
	printf("\n %d \n", BITMAP_SIZE_B);
	bitarray = bitarray_create((char*) bitmap_start, BITMAP_SIZE_B);

	// Borra hasta que los nodos de posicion coincidan con los nodos especificados.
	while( (data_pos != data_upto) | (pointer_pos != pointer_upto) | ((data_pos == 0) & (pointer_pos == 0)) ){
		if ((data_pos < 0) | (pointer_pos < 0)) break;
		if (data_pos != 0){
			// Ubica y borra el nodo correspondiente
			aux = &(file_data->blk_indirect[pointer_pos]);
			node_to_delete = aux[data_pos];
			bitarray_clean_bit(bitarray, node_to_delete);
			bitmap_free_blocks++;

			// Reubica el offset
			data_pos--;
		}

		// Si el data_offset es 0, debe borrar la estructura de punteros
		else if (data_pos == 0){
//			if ((data_pos == 0) & (pointer_pos ==0)) return 0;

			node_pointer_to_delete = file_data->blk_indirect[pointer_pos]; // Ubica el numero de nodo de punteros a borrar.
			aux = (ptrGBloque *) &(header_start[node_pointer_to_delete]); // Entra a dicho nodo de punteros.
			node_to_delete = aux[0];	// Selecciona el nodo 0 de dicho bloque de punteros.
			bitarray_clean_bit(bitarray, node_pointer_to_delete);
			bitarray_clean_bit(bitarray, node_to_delete);
			file_data->blk_indirect[pointer_pos] = 0; // Se utiliza el 0 como referencia a bloque no indicado.

			// Reubica el offset
			data_pos = 1023;
			pointer_pos--;
		}


	}

	// Cierra el bitmap
	bitarray_destroy(bitarray);
	return 0;
}

/*
 *	@DESC
 *		Obtiene espacio nuevo para un archivo, agregandole los nodos que sean necesarios.
 *		Actualiza el FileSize al tamanio correspondiente.
 *
 *	@PARAM
 *		file_data - El puntero al nodo donde se encuentra el archivo.
 *		size - El tamanio que se le debe agregar.
 *
 *	@RET
 *		0 - Se consiguio el espacio requerido
 *		negativo - Error.
 */
int get_new_space (struct grasa_file_t *file_data, int size){
	size_t file_size = file_data->file_size, space_in_block = file_size % BLOCKSIZE;
	int new_node;
	space_in_block = BLOCKSIZE - space_in_block; // Calcula cuanto tamanio le queda para ocupar en el bloque

	// Si no hay suficiente espacio, retorna error.
	if ((bitmap_free_blocks*BLOCKSIZE) < (size - space_in_block)) return -1;

	// Actualiza el file size al tamanio que le corresponde:
	if (space_in_block >= size){
		file_data->file_size += size;
		return 0;
	} else {
		file_data->file_size += space_in_block;
	}

	while ( (space_in_block <= size) ){ // Siempre que lo que haya que escribir sea mas grande que el espacio que quedaba en el bloque
		new_node = get_node();
		add_node(file_data, new_node);
		size -= BLOCKSIZE;
		file_data->file_size += BLOCKSIZE;
	}

	file_data->file_size += size;

	return 0;
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
 */
int grasa_rmdir (const char* path){
			log_trace(logger, "Rmdir: Path: %s", path);
	int nodo_padre = determinar_nodo(path), i, res = 0;
	if (nodo_padre < 0) return -ENOENT;
	struct grasa_file_t *node;

	// Toma un lock de escritura.
			log_lock_trace(logger, "Rmdir: Pide lock escritura. Escribiendo: %d. En cola: %d.", rwlock.__data.__writer, rwlock.__data.__nr_writers_queued);
	pthread_rwlock_wrlock(&rwlock);
			log_lock_trace(logger, "Rmdir: Recibe lock escritura.");
	// Abre conexiones y levanta la tabla de nodos en memoria.
	node = bitmap_start;

	node = &(node[nodo_padre]);

	// Chequea si el directorio esta vacio. En caso que eso suceda, FUSE se encarga de borrar lo que hay dentro.
	for (i=0; i < 1024 ;i++){
		if (((&node_table_start[i])->state != DELETED_T) & ((&node_table_start[i])->parent_dir_block == nodo_padre)) {
			res = -ENOTEMPTY;
			goto finalizar;
		}
	}

	node->state = DELETED_T; // Aca le dice que el estado queda "Borrado"


	// Cierra, ponele la alarma y se va para su casa. Mejor dicho, retorna 0 :D
	finalizar:
	// Devuelve el lock de escritura.
	pthread_rwlock_unlock(&rwlock);
			log_lock_trace(logger, "Rmdir: Devuelve lock escritura. En cola: %d", rwlock.__data.__nr_writers_queued);
	return res;
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
 */
int grasa_truncate (const char *path, off_t new_size){
			log_info(logger, "Truncate: Path: %s - New size: %d", path, new_size);
	int nodo_padre = determinar_nodo(path);
	if (nodo_padre < 0) return -ENOENT;
	struct grasa_file_t *node;

	// Toma un lock de escritura.
			log_lock_trace(logger, "Truncate: Pide lock escritura. Escribiendo: %d. En cola: %d.", rwlock.__data.__writer, rwlock.__data.__nr_writers_queued);
	pthread_rwlock_wrlock(&rwlock);
			log_lock_trace(logger, "Truncate: Recibe lock escritura.");
	// Abre conexiones y levanta la tabla de nodos en memoria.
	node = bitmap_start;

	node = &(node[nodo_padre]);

	// Si el nuevo size es mayor, se deben reservar los nodos correspondientes:
	if (new_size > node->file_size){
		get_new_space(node, (new_size - node->file_size));

	} else {	// Si no, se deben borrar los nodos hasta ese punto.
		int pointer_to_delete;
		int data_to_delete;

		set_position(&pointer_to_delete, &data_to_delete, 0, new_size);

		delete_nodes_upto(node, pointer_to_delete, data_to_delete);
	}

	node->file_size = new_size; // Aca le dice su nuevo size.

	// Como el truncar borra todos los nodos si tiene tamanio 0, se le debe asignar un nuevo nodo para que pueda abrir correctamente.
	if (new_size == 0){
		int new_node = get_node();
		add_node(node, new_node);
	}


	// Cierra, ponele la alarma y se va para su casa. Mejor dicho, retorna 0 :D
	// Devuelve el lock de escritura.
	pthread_rwlock_wrlock(&rwlock);
			log_lock_trace(logger, "Truncate: Devuelve lock escritura. En cola: %d", rwlock.__data.__nr_writers_queued);
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
			log_trace(logger, "Writing: Path: %s - Size: %d - Offset %d", path, size, offset);
	(void) fi;
	int nodo = determinar_nodo(path);
	int new_free_node;
	struct grasa_file_t *node;
	char *data_block;
	size_t tam = size, file_size, space_in_block, offset_in_block = offset % BLOCKSIZE;
	off_t off = offset;
	int *n_pointer_block = malloc(sizeof(int)), *n_data_block = malloc(sizeof(int));
	ptrGBloque *pointer_block;

	// Toma un lock de escritura.
			log_lock_trace(logger, "Write: Pide lock escritura. Escribiendo: %d. En cola: %d.", rwlock.__data.__writer, rwlock.__data.__nr_writers_queued);
	pthread_rwlock_wrlock(&rwlock);
			log_lock_trace(logger, "Write: Recibe lock escritura.");
	// Abrir conexion y traer directorios, guarda el bloque de inicio para luego liberar memoria
	node = node_table_start;

	// Ubica el nodo correspondiente al archivo
	node = &(node[nodo-1]);
	file_size = node->file_size;
	space_in_block = BLOCKSIZE - (file_size % BLOCKSIZE);
	if (space_in_block == BLOCKSIZE) (space_in_block = 0); // Porque significa que el bloque esta lleno.

	// Guarda tantas veces como sea necesario, consigue nodos y actualiza el archivo.
	while (tam != 0){

		// Ubica a que nodo le corresponderia guardar el dato
		set_position(n_pointer_block, n_data_block, file_size, off);

		// Si el offset es mayor que el tamanio del archivo mas el resto del bloque libre, significa que hay que pedir un bloque nuevo
		if ((off >= (file_size + space_in_block)) & (file_size != 0)){

			// Si no hay espacio en el disco, retorna error.
			if (bitmap_free_blocks == 0) return -ENOSPC;

			// Obtiene un bloque libre para escribir.
			new_free_node = get_node();

			// Actualiza la informacion del archivo.
			add_node(node, new_free_node);

			// Lo relativiza al data block.
			new_free_node -= (GHEADERBLOCKS + NODE_TABLE_SIZE + BITMAP_BLOCK_SIZE);
			data_block = (char*) &(data_block_start[new_free_node]);

		} else {
			//Ubica el nodo a escribir.
			*n_pointer_block = node->blk_indirect[*n_pointer_block];
			*n_pointer_block -= (GHEADERBLOCKS + NODE_TABLE_SIZE + BITMAP_BLOCK_SIZE);
			pointer_block = (ptrGBloque*) &(data_block_start[*n_pointer_block]);
			*n_data_block = pointer_block[*n_data_block];
			*n_data_block -= (GHEADERBLOCKS + NODE_TABLE_SIZE + BITMAP_BLOCK_SIZE);
			data_block = (char*) &(data_block_start[*n_data_block]);
		}

		// Escribe en ese bloque de datos.
		if (tam >= BLOCKSIZE){
			memcpy(data_block, buf, BLOCKSIZE);
			if ((node->file_size) <= (off)) file_size = node->file_size += BLOCKSIZE;
			off += BLOCKSIZE;
			tam -= BLOCKSIZE;
		} else {
			memcpy(data_block + offset_in_block, buf, tam);
			if (node->file_size <= off) file_size = node->file_size += tam;
			else if (node->file_size <= (off + tam)) file_size = node->file_size += (off + tam - node->file_size);
			tam = 0;
		}

	}

	// Devuelve el lock de escritura.
	pthread_rwlock_unlock(&rwlock);
			log_lock_trace(logger, "Write: Devuelve lock escritura. En cola: %d", rwlock.__data.__nr_writers_queued);

			log_trace(logger, "Terminada escritura.");
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
		log_info(logger, "Mknod: Path: %s", path);
	int nodo_padre, i, res = 0;
	int new_free_node;
	struct grasa_file_t *node;
	char *nombre = malloc(strlen(path) + 1), *nom_to_free = nombre;
	char *dir_padre = malloc(strlen(path) + 1), *dir_to_free = dir_padre;
	char *data_block;

	split_path(path, &dir_padre, &nombre);

	// Ubica el nodo correspondiente. Si es el raiz, lo marca como 0, Si es menor a 0, lo crea (mismos permisos).
	if (strcmp(dir_padre, "/") == 0){
		nodo_padre = 0;
	} else if ((nodo_padre = determinar_nodo(dir_padre)) < 0){
		grasa_mkdir(path, mode);
	}


	node = node_table_start;

	// Toma un lock de escritura.
			log_lock_trace(logger, "Mknod: Pide lock escritura. Escribiendo: %d. En cola: %d.", rwlock.__data.__writer, rwlock.__data.__nr_writers_queued);
	pthread_rwlock_wrlock(&rwlock);
			log_lock_trace(logger, "Mknod: Recibe lock escritura.");

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
	node->file_size = 0; // El tamanio se ira sumando a medida que se escriba en el archivo.
	node->parent_dir_block = nodo_padre;
	node->blk_indirect[0] = 0; // Se utiliza esta marca para avisar que es un archivo nuevo. De esta manera, la funcion add_node conoce que esta recien creado.
	res = 0;

	// Obtiene un bloque libre para escribir.
	new_free_node = get_node();

	// Actualiza la informacion del archivo.
	add_node(node, new_free_node);

	// Lo relativiza al data block.
	new_free_node -= (GHEADERBLOCKS + NODE_TABLE_SIZE + BITMAP_BLOCK_SIZE);
	data_block = (char*) &(data_block_start[new_free_node]);

	// Escribe en ese bloque de datos.
	memset(data_block, '\0', BLOCKSIZE);

	finalizar:
	free(nom_to_free);
	free(dir_to_free);

	// Devuelve el lock de escritura.
	pthread_rwlock_unlock(&rwlock);
			log_lock_trace(logger, "Mknod: Devuelve lock escritura. En cola: %d", rwlock.__data.__nr_writers_queued);
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
	struct grasa_file_t* file_data;
	int node = determinar_nodo(path);

	file_data = &(node_table_start[node - 1]);

	delete_nodes_upto(file_data, 0, 0);

	return grasa_rmdir(path);
	}

/*
 *
 */
int grasa_rename (const char* oldpath, const char* newpath){

			log_info(logger, "Rename: Moviendo archivo. From: %s - To: %s", oldpath, newpath);
	char* newroute = malloc(strlen(newpath) + 1);
	char* newname = malloc(GFILENAMELENGTH + 1);
	char* tofree1 = newroute;
	char* tofree2 = newname;
	split_path(newpath, &newroute, &newname);
	int old_node = determinar_nodo(oldpath), new_parent_node = determinar_nodo(newroute);

	// Modifica los valores del file. Como el determinar_nodo devuelve el numero de nodo +1, lo reubica.
	strcpy((char *) &(node_table_start[old_node - 1].fname[0]), newname);
	node_table_start[old_node -1].parent_dir_block = new_parent_node;

	free(tofree1);
	free(tofree2);

	return 0;
}

/*
 * 	@DESC
 * 		Obtiene y registra la cantidad de bloques de datos libres.
 */
int obtain_free_blocks(){
	t_bitarray *bitarray;
	int free_nodes=0, i;
	int bitmap_size_in_bits = BITMAP_SIZE_BITS;

	bitarray = bitarray_create((char*) bitmap_start, BITMAP_SIZE_B);

	for (i = 0; i < bitmap_size_in_bits; i++){
		if (bitarray_test_bit(bitarray, i) == 0) free_nodes++;
	}

	bitarray_destroy(bitarray);

	bitmap_free_blocks = free_nodes;

	return free_nodes;
}

/*
 *  DESC
 *  	Settea los permisos de acceso a un file
 *
 *  PARAM
 *  	path - path del archivo
 *  	flags - flags que corresponden a los permisos del archivo
 *
 *  RET
 *  	0 - Access granted
 *  	-1 - Access denied
 */
int grasa_access(){

	return 0;
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
		.mkdir = grasa_mkdir,		// OK
		.rmdir = grasa_rmdir,		// OK
		.truncate = grasa_truncate, // OK
		.write = grasa_write,		// OK
		.mknod = grasa_mknod,		// OK
		.unlink = grasa_unlink,		// OK
		.rename = grasa_rename,
};

/** keys for FUSE_OPT_ options */
enum {
	KEY_VERSION,
	KEY_HELP,
};


/*
 * Esta estructura es utilizada para decirle a la biblioteca de FUSE que
 * parametro puede recibir y donde tiene que guardar el valor de estos
 */
static struct fuse_opt fuse_options[] = {

		// Si se le manda el parametro "--Disc-Path", lo utiliza:
		CUSTOM_FUSE_OPT_KEY("--Disc-Path=%s", define_disc_path, 0),

		// Define el log level
		CUSTOM_FUSE_OPT_KEY("--ll=%s", log_level_param, 0),

		// Define el log path
		CUSTOM_FUSE_OPT_KEY("--Log-Path", log_path_param, 0),

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

void sig_int_handler(int sig){
	log_info(logger, "Recibido signal SIGUSR1");
	if (sig == SIGUSR1) {
		printf("\n%d\n", obtain_free_blocks());
	}
	log_trace(logger, "SIGUSR1 res: %d", bitmap_free_blocks);
}

void sig_term_handler(int sig){
	log_error(logger, "Programa terminado anormalmente con signal %d", sig);
	// Termina el programa de forma normal.
	if (sig == SIGTERM){
		// Destruye el lock:
			pthread_rwlock_destroy(&rwlock);

			// Destruye el log
			log_destroy(logger);

			// Cierra lo que tiene en memoria.
			if (munmap(header_start, ACTUAL_DISC_SIZE_B ) == -1) printf("ERROR");

			close(discDescriptor);
	}
}

int main (int argc, char *argv[]){

	signal(SIGUSR1, sig_int_handler);
	signal(SIGTERM, sig_term_handler);
	signal(SIGABRT, sig_term_handler);

	int res, fd;

	// Crea los atributos del rwlock
	pthread_rwlockattr_t attrib;
	pthread_rwlockattr_init(&attrib);
	pthread_rwlockattr_setpshared(&attrib, PTHREAD_PROCESS_SHARED);
	// Crea el lock
	pthread_rwlock_init(&rwlock, &attrib);

	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);

	// Limpio la estructura que va a contener los parametros
	memset(&runtime_options, 0, sizeof(struct t_runtime_options));

	// Esta funcion de FUSE lee los parametros recibidos y los intepreta
	if (fuse_opt_parse(&args, &runtime_options, fuse_options, NULL) == -1){
		/** error parsing options */
		perror("Invalid arguments!");
		return EXIT_FAILURE;
	}

	// Setea el path del disco
	if (runtime_options.define_disc_path != NULL){
		strcpy(fuse_disc_path, runtime_options.define_disc_path);
	} else{
		strcpy(fuse_disc_path, "/home/utnso/tp-2013-2c-c-o-no-ser/FileSystem/Testdisk/disk.bin");
	}

	// Settea el log level del disco:
	t_log_level log_level = LOG_LEVEL_TRACE;
	if (runtime_options.log_level_param != NULL){
		if (!strcmp(runtime_options.log_level_param, "LockTrace")) log_level = LOG_LEVEL_LOCK_TRACE;
		else if (!strcmp(runtime_options.log_level_param, "Trace")) log_level = LOG_LEVEL_TRACE;
		else if (!strcmp(runtime_options.log_level_param, "Debug")) log_level = LOG_LEVEL_DEBUG;
		else if (!strcmp(runtime_options.log_level_param, "Info")) log_level = LOG_LEVEL_INFO;
		else if (!strcmp(runtime_options.log_level_param, "Warning")) log_level = LOG_LEVEL_WARNING;
		else if (!strcmp(runtime_options.log_level_param, "Error")) log_level = LOG_LEVEL_ERROR;
		else log_level = LOG_LEVEL_TRACE;
	}

	// Settea el log path
	if (runtime_options.log_path_param != NULL){
		strcpy(fuse_log_path,runtime_options.log_path_param);
	} else {
		strcpy(fuse_log_path,"/home/utnso/tp-2013-2c-c-o-no-ser/FileSystem/log/");
	}

	// Obiene el tamanio del disco
	fuse_disc_size = path_size_in_bytes(DISC_PATH);

	// Abrir conexion y traer directorios, guarda el bloque de inicio para luego liberar memoria
	if ((discDescriptor = fd = open(DISC_PATH, O_RDWR, 0)) == -1) printf("ERROR");
	header_start = (struct grasa_header_t*) mmap(NULL, ACTUAL_DISC_SIZE_B , PROT_WRITE | PROT_READ | PROT_EXEC, MAP_SHARED, fd, 0);
	Header_Data = *header_start;
	bitmap_start = (struct grasa_file_t*) &header_start[GHEADERBLOCKS];
	node_table_start = (struct grasa_file_t*) &header_start[GHEADERBLOCKS + BITMAP_BLOCK_SIZE];
	data_block_start = (struct grasa_file_t*) &header_start[GHEADERBLOCKS + BITMAP_BLOCK_SIZE + NODE_TABLE_SIZE];


	// Crea el log:
	logger = log_create(strcat(LOG_PATH,"Log.txt"), "Grasa Filesystem", 1, log_level);

	log_info(logger, "Log inicializado correctamente");

	// Cuenta y registra la cantidad de nodos libres.
	obtain_free_blocks();

	// Esta es la funcion principal de FUSE, es la que se encarga
	// de realizar el montaje, comuniscarse con el kernel, delegar
	// en varios threads
	log_info(logger, "Se ingresa al modulo de FUSE");

	res = fuse_main(args.argc, args.argv, &grasa_oper, NULL);

	// Destruye el lock:
	pthread_rwlock_destroy(&rwlock);
	pthread_rwlockattr_destroy(&attrib);

	// Destruye el log
	log_destroy(logger);

	// Cierra lo que tiene en memoria.
	if (munmap(header_start, ACTUAL_DISC_SIZE_B ) == -1) printf("ERROR");

	close(fd);

	return res;

}
