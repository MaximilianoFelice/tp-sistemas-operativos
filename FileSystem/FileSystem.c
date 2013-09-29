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
#define BLOCKSIZE 4096
#define NODE_TABLE_SIZE 1024
#define BITMAP_SIZE
#define DISC_PATH "/home/utnso/grasa-tools/disco.bin"

/*
 * @DESCRIPCION
 *
 * 				Esta funcion imprime los archivos y directorios que se encuentran en el filesystem.
 */

int main (int argc, char *argv[]){

	struct grasa_header_t block;
	struct grasa_file_t file;
	int* bitmap,i;
	int fd;

	fd = open(DISC_PATH, O_RDWR, 0);
	printf("Numero de Conexion:%d \n\n", fd);

	//Lee el primer bloque, donde se encuentra el Header
	read(fd, &block, sizeof(block));
	printf("ID: %s \n VERSION: %d \n BLOQUE DE INICIO DE BITMAP: %d \n TAMAÑO BITMAP (BLOQUES): %d \n", block.grasa,block.version, block.blk_bitmap, block.size_bitmap);

	bitmap = malloc( BLOCKSIZE * block.size_bitmap);
	//Lee el bitmap (lo saltea, ya que en este momento no nos importa).
	read(fd,bitmap,BLOCKSIZE * block.size_bitmap);

	//Lee los registros dentro de la tabla de nodos hasta que se termine (GFILEBYTABLE=1024 bloques).
	//Imprime y discrimina si es un archivo (state=1) o si es un directorio (state=2)
	for(i=0;i!=GFILEBYTABLE;i++){
		read(fd,&file,sizeof(file));
		if (file.state == 2) printf("%d - Es un directorio llamado: %s", file.state, file.fname);
		if (file.state == 1) printf("%d - Es un archivo llamado: %s", file.state, file.fname);
	}

	//Cierra conexiones y libera memoria.
	close(fd);
	free(bitmap);

return 0;
}
