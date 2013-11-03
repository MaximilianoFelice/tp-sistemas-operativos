//RECORDAR INCLUIR STDINT, QUE CONTIENE LAS DEFINICIONES DE LOS TIPOS DE DATOS USADOS.
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/mman.h>
#include <fcntl.h>

#define GFILEBYTABLE 1024
#define GFILEBYBLOCK 1
#define GFILENAMELENGTH 71
#define GHEADERBLOCKS 1
#define BLKINDIRECT 1000
#define BLOCKSIZE 4096

// Macros que definen los tamanios de los archivos.
#define NODE_TABLE_SIZE 1024
#define NODE_TABLE_SIZE_B ((int) NODE_TABLE_SIZE * BLOCKSIZE)
#define DISC_PATH fuse_disc_path
#define DISC_SIZE_B(p) path_size_in_bytes(p)
#define ACTUAL_DISC_SIZE_B fuse_disc_size
#define BITMAP_SIZE_B get_size()
#define HEADER_SIZE_B ((int) GHEADERBLOCKS * BLOCKSIZE)
#define BITMAP_BLOCK_SIZE Header_Data.size_bitmap

// Macros de movimiento dentro del archivo.
//#define AVANZAR_BLOQUES(node,cant) advance(node,cant)
#define OPEN_HEADER(fd) goto_Header(fd)
#define OPEN_BITMAP(fd) goto_Bitmap(fd)
#define OPEN_NODE_TABLE(fd) goto_Node_Table(fd)
#define OPEN_DATA_BLOCK(fd) goto_Data_Block(fd)

// Definiciones de tipo de bloque borrado(0), archivo(1), directorio(2)
#define DELETED_T ((int) 0)
#define FILE_T ((int) 1)
#define DIRECTORY_T ((int) 2)

// Definiciones para el manejo de bits
#define BITMAP_TYPE char
#define READTHEBIT(n_bit, inicio) BITPOS(GETBYTE(n_bit), OFFSET(n_bit), inicio)
#define GETBYTE(n_bit) ((int) n_bit / CHAR_BIT)
#define OFFSET(n_bit) ((int) n_bit % CHAR_BIT)
#define BITPOS(n_byte, offset, inicio) getbit(n_byte, offset, inicio)

// Se guardara aqui la ruta al disco. Tiene un tamanio maximo.
char fuse_disc_path[1000];

// Se guardara aqui el tamanio del disco
int fuse_disc_size;

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

typedef struct grasa_file_t { // un cuarto de bloque (256 bytes)
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

void* goto_Header(int fd){
	struct grasa_file_t *node;
	node = (void*) mmap(NULL, HEADER_SIZE_B , PROT_WRITE | PROT_READ | PROT_EXEC, MAP_SHARED, fd, 0);
	return node;

}

void* goto_Bitmap(int fd){
	struct grasa_file_t *node;
	node = (void*) mmap(NULL, HEADER_SIZE_B , PROT_WRITE | PROT_READ | PROT_EXEC, MAP_SHARED, fd, 0);
	node = &(node[GFILEBYBLOCK]);

	return node;
}

void* goto_Node_Table(int fd){
	struct grasa_file_t *node;
	node = (void*) mmap(NULL, HEADER_SIZE_B + BITMAP_SIZE_B , PROT_WRITE | PROT_READ | PROT_EXEC, MAP_SHARED, fd, 0);
	node = &(node[GFILEBYBLOCK + BITMAP_BLOCK_SIZE]);
	return node;

}

void* goto_Data_Block(int fd){
	struct grasa_file_t *node;
	node = (void*) mmap(NULL, ACTUAL_DISC_SIZE_B , PROT_WRITE | PROT_READ | PROT_EXEC, MAP_SHARED, fd, 0);
	node = &(node[GFILEBYBLOCK + BITMAP_BLOCK_SIZE + GFILEBYTABLE]);
	return node;
}

int getbit(int n_byte, int offset, BITMAP_TYPE* inicio){
	int i, tam_byte = CHAR_BIT; // tam_byte = sizeof(BITMAP_TYPE) * CHAR_BIT;

	// Ubica el byte en el que se buscara el bit
	inicio = &(inicio[n_byte]);

	// Ubica el bit
	for (i = 1; tam_byte != 0; (tam_byte --, i *= 2));
	for(; (offset != 0); (offset--, i/=2));
	if ((*inicio & i) != i) return 1;
	return 0;
}
