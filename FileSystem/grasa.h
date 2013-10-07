#define GFILEBYTABLE 1024
#define GFILEBYBLOCK 1
#define GFILENAMELENGTH 71
#define GHEADERBLOCKS 1
#define BLKINDIRECT 1000
#define BLOCKSIZE 4096

// Macros que definen los tamanios de los archivos.
#define NODE_TABLE_SIZE 1024
#define NODE_TABLE_SIZE_B ((int) NODE_TABLE_SIZE * BLOCKSIZE)
#define DISC_PATH "/home/utnso/grasa-tools/disco.bin"
#define DISC_SIZE_B(p) path_size_in_bytes(p)
#define ACTUAL_DISC_SIZE_B path_size_in_bytes(DISC_PATH)
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

//RECORDAR INCLUIR STDINT, QUE CONTIENE LAS DEFINICIONES DE LOS TIPOS DE DATOS USADOS.
#include <stdint.h>
#include <stdio.h>
#include <sys/mman.h>
#include <fcntl.h>

typedef uint32_t ptrGBloque;


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
	return ((int) (DISC_SIZE_B(DISC_PATH) / BLOCKSIZE));
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

