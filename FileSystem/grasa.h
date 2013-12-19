//RECORDAR INCLUIR STDINT, QUE CONTIENE LAS DEFINICIONES DE LOS TIPOS DE DATOS USADOS.

#include <stdint.h>
#include <stdio.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <limits.h>		// Aqui obtenemos el CHAR_BIT, que nos permite obtener la cantidad de bits en un char.
#include <fuse.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stddef.h>
#include <errno.h>
#include <unistd.h>
#include <sys/mman.h>
#include <commons/bitarray.h>
#include <semaphore.h>
#include <pthread.h>
#include <commons/log.h>
#include <signal.h>
#include <commons/string.h>
#include "grasa_cache.h"

/* Opciones de FUSE. Esta redefinicion le indica cuales son las opciones que se utilizaran. */
#define FUSE_USE_VERSION 27
#define _FILE_OFFSET_BITS 64

/* Fin opciones de fuse */

#define GFILEBYTABLE 1024
#define GFILEBYBLOCK 1
#define GFILENAMELENGTH 71
#define GHEADERBLOCKS 1
#define BLKINDIRECT 1000
#define BLOCKSIZE 4096
#define PTRGBLOQUE_SIZE 1024

#define THELARGESTFILE (uint32_t) (BLKINDIRECT*PTRGBLOQUE_SIZE*BLOCKSIZE)

// Macros que definen los tamanios de los bloques.
#define NODE_TABLE_SIZE 1024
#define NODE_TABLE_SIZE_B ((int) NODE_TABLE_SIZE * BLOCKSIZE)
#define DISC_PATH fuse_disc_path
#define DISC_SIZE_B(p) path_size_in_bytes(p)
#define ACTUAL_DISC_SIZE_B fuse_disc_size
#define BITMAP_SIZE_B (int) (get_size() / CHAR_BIT)
#define BITMAP_SIZE_BITS get_size()
#define HEADER_SIZE_B ((int) GHEADERBLOCKS * BLOCKSIZE)
#define BITMAP_BLOCK_SIZE Header_Data.size_bitmap

// Macros de movimiento dentro del archivo.
//#define AVANZAR_BLOQUES(node,cant) advance(node,cant)
#define OPEN_HEADER(fd) goto_Header(fd)


// Definiciones de tipo de bloque borrado(0), archivo(1), directorio(2)
#define DELETED_T ((int) 0)
#define FILE_T ((int) 1)
#define DIRECTORY_T ((int) 2)

// Se utiliza esta variable para saber si se encuentra en modo "borrar". Esto afecta, principalmente, al delete_nodes_upto
#define DELETE_MODE _del_mode
#define ENABLE_DELETE_MODE _del_mode=1
#define DISABLE_DELETE_MODE _del_mode=0
int _del_mode;

// Se guarda el tamaño del bitarray para la implementacion de 64 bits.
#define ARRAY64SIZE _bitarray_64
size_t _bitarray_64;
#define ARRAY64LEAK _bitarray_64_leak
size_t _bitarray_64_leak;

// Se guardara aqui la ruta al disco. Tiene un tamanio maximo.
char fuse_disc_path[1000];

// Se guardara aqui el tamanio del disco
int fuse_disc_size;

// Se guardara aqui la cantidad de bloques libres en el bitmap
int bitmap_free_blocks;

typedef uint32_t ptrGBloque;

typedef ptrGBloque pointer_data_block [PTRGBLOQUE_SIZE];


typedef struct grasa_header_t { // un bloque
	unsigned char grasa[5];
	uint32_t version;
	uint32_t blk_bitmap;
	uint32_t size_bitmap; // en bloques
	unsigned char padding[4073];
} GHeader;

struct grasa_header_t Header_Data;

typedef struct grasa_file_t {
	uint8_t state; // 0: borrado, 1: archivo, 2: directorio
	unsigned char fname[GFILENAMELENGTH];
	uint32_t parent_dir_block;
	uint32_t file_size;
	uint64_t c_date;
	uint64_t m_date;
	ptrGBloque blk_indirect[BLKINDIRECT];
} GFile;

// Definimos el semaforo que se utilizará para poder escribir:
pthread_rwlock_t rwlock;
t_log* logger;

/*
 * Este es el path de nuestro, relativo al punto de montaje, archivo dentro del FS
 */
#define DEFAULT_FILE_PATH "/" DEFAULT_FILE_NAME
#define CUSTOM_FUSE_OPT_KEY(t, p, v) { t, offsetof(struct t_runtime_options, p), v }

// Define los datos del log
#define LOG_PATH fuse_log_path
char fuse_log_path[1000];


// Define los datos de mappeo de memoria:
struct grasa_header_t *header_start;
struct grasa_file_t *node_table_start, *data_block_start, *bitmap_start;

// Utiliza esta estructura para almacenar el numero de descriptor en el cual se abrio el disco
int discDescriptor;

/* DEFINICION DE LAS FUNCIONES QUE SE USARAN EN ESTA IMPLEMENTACION */

	// Funcines auxiliares de manejo de estructuras (incluidas en <Grasa_Handlers.c>)
	ptrGBloque determinar_nodo(const char*);
	int get_node(void);
	int add_node(struct grasa_file_t *, int);
	int delete_nodes_upto (struct grasa_file_t*, int, int);
	int set_position (int*, int*, size_t, off_t);
	int get_new_space (struct grasa_file_t*, int);
	int obtain_free_blocks(void);
	int split_path(const char*, char**, char**);
	int path_size_in_bytes(const char*);
	int get_size(void);
	uint32_t get_node_number(const char*, struct fuse_file_info*);

	// Funciones de lectura de FUSE (incluidas en <Grasa_Read.c>)
	int grasa_getattr(const char*, struct stat*);
	int grasa_readdir(const char*, void *buf, fuse_fill_dir_t, off_t, struct fuse_file_info*);
	int grasa_read(const char*, char *, size_t, off_t, struct fuse_file_info*);

	// Funciones de escritura de FUSE (incluidas en <Grasa_Write.c>)
	int grasa_mkdir (const char*, mode_t);
	int grasa_rmdir (const char*);
	int grasa_truncate (const char*, off_t);
	int grasa_write (const char*, const char*, size_t, off_t, struct fuse_file_info*);
	int grasa_mknod (const char*, mode_t, dev_t);
	int grasa_unlink (const char*);
	int grasa_rename (const char*, const char*);
	int grasa_setxattr(const char*, const char*, const char*, size_t, int);
	int grasa_utime(const char*, struct utimbuf*);

	// Funciones definidas como Dummy porque la implementacion no las requiere (incluidas en <Grasa_Dummy.c>)
	int grasa_open(const char*, struct fuse_file_info*);
	int grasa_access(const char*, int);
	int grasa_chmod(const char*, mode_t);
	int grasa_chown(const char*, uid_t, gid_t);

/* FIN DE DEFINICION DE FUNCIONES */




