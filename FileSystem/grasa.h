//RECORDAR INCLUIR STDINT, QUE CONTIENE LAS DEFINICIONES DE LOS TIPOS DE DATOS USADOS.
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <limits.h>		// Aqui obtenemos el CHAR_BIT, que nos permite obtener la cantidad de bits en un char.


#define GFILEBYTABLE 1024
#define GFILEBYBLOCK 1
#define GFILENAMELENGTH 71
#define GHEADERBLOCKS 1
#define BLKINDIRECT 1000
#define BLOCKSIZE 4096

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


// Se guardara aqui la ruta al disco. Tiene un tamanio maximo.
char fuse_disc_path[1000];

// Se guardara aqui el tamanio del disco
int fuse_disc_size;

// Se guardara aqui la cantidad de bloques libres en el bitmap
int bitmap_free_blocks;

typedef uint32_t ptrGBloque;

typedef ptrGBloque pointer_data_block [1024];


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


int path_size_in_bytes(const char* path){
	FILE *fd;
	int size;

	fd=fopen(path, "r"); // printf("Error al abrir el archivo calculando el tamanio");

	fseek(fd, 0L, SEEK_END);
	size = ftell(fd);

	fclose(fd);

	return size;
}

int get_size(){
	return ((int) (ACTUAL_DISC_SIZE_B / BLOCKSIZE));
}



