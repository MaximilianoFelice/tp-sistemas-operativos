/*
 * Grasa_Read.c
 *
 *  Created on: 22/11/2013
 *      Author: utnso
 */

#include "grasa.h"

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
 * 			stbuf->st_mode = S_IFDIR | 0777;
 * 			stbuf->st_nlink = 2;
 * 		Si es un archivo:
 * 			stbuf->st_mode = S_IFREG | 0777;
 * 			stbuf->st_nlink = 1;
 * 			stbuf->st_size = [TAMANIO];
 *
 */
int grasa_getattr(const char *path, struct stat *stbuf) {
			log_info(logger, "Getattr: Path: %s", path);
	int nodo = determinar_nodo(path), res;
	if (nodo < 0) return -ENOENT;
	struct grasa_file_t *node;
	memset(stbuf, 0, sizeof(struct stat));

	if (nodo == -1) return -ENOENT;

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
		stbuf->st_atime = time(NULL); /* Le decimos que el access time es la hora actual */
		res = 0;
		goto finalizar;
	} else if(node->state == 1){
		stbuf->st_mode = S_IFREG | 0777;
		stbuf->st_nlink = 1;
		stbuf->st_size = node->file_size;
		stbuf->st_mtime = node->m_date;
		stbuf->st_ctime = node->c_date;
		stbuf->st_atime = time(NULL); /* Le decimos que el access time es la hora actual */
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
int grasa_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {
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
		if ((nodo==(node->parent_dir_block)) & (((node->state) == DIRECTORY_T) | ((node->state) == FILE_T)))  filler(buf, (char*) &(node->fname[0]), NULL, 0);
		node = &node[1];
	}


	pthread_rwlock_unlock(&rwlock); //Devuelve un lock de lectura.
			log_lock_trace(logger, "Readdir: Libera lock lectura. Cantidad de lectores: %d", rwlock.__data.__nr_readers);
	return res;
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
int grasa_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
			log_info(logger, "Reading: Path: %s - Size: %d - Offset %d", path, size, offset);
	(void) fi;
	unsigned int nodo = determinar_nodo(path), bloque_punteros, num_bloque_datos;
	unsigned int bloque_a_buscar; // Estructura auxiliar para no dejar choclos
	struct grasa_file_t *node;
	ptrGBloque *pointer_block;
	char *data_block;
	size_t tam = size;
	int res;

	if (nodo == -1) return -ENOENT;

	node = node_table_start;

	// Ubica el nodo correspondiente al archivo
	node = &(node[nodo-1]);

	pthread_rwlock_rdlock(&rwlock); //Toma un lock de lectura.
			log_lock_trace(logger, "Read: Toma lock lectura. Cantidad de lectores: %d", rwlock.__data.__nr_readers);

	if(node->file_size <= offset){
		log_error(logger, "Fuse intenta leer un offset mayor o igual que el tamanio de archivo. Se retorna size 0. File: %s, Size: %d", path, node->file_size);
		res = 0;
		goto finalizar;
	} else if (node->file_size <= (offset+size)){
		tam = size = ((node->file_size)-(offset));
		log_error(logger, "Fuse intenta leer una posicion mayor o igual que el tamanio de archivo. Se retornaran %d bytes. File: %s, Size: %d", size, path, node->file_size);
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
