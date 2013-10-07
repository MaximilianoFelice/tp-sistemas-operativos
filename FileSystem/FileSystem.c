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
#include <commons/string.h>


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

// Funcion auxiliar para poder probar cosas hasta que nos manden el disco.
void escribir_uno(){
	int fd;
	struct grasa_file_t *node, *inicio;
	unsigned char *aux;

	// Abrir conexion y traer directorios, guarda el bloque de inicio para luego liberar memoria
	if ((fd = open(DISC_PATH, O_RDWR, 0)) == -1) printf("ERROR");
	node = (void*) mmap(NULL, HEADER_SIZE_B + BITMAP_SIZE_B + NODE_TABLE_SIZE_B , PROT_WRITE | PROT_READ | PROT_EXEC, MAP_SHARED, fd, 0);
	inicio = node;
	node = &(node[GFILEBYBLOCK + BITMAP_BLOCK_SIZE]);

	// Escribe en el bloque 1 el directorio
	node->parent_dir_block = 0;
	aux = &(node->fname[0]);
	strcpy((char*) aux, "Carpeta");
	node->state = 2;
	node->file_size = 154;

	node = &(node[1]);
	node->parent_dir_block = 0;
	aux = &(node->fname[0]);
	strcpy((char*) aux, "Otra Carpetita");
	node->state = 2;
	node->file_size = 154234;

	node = &(node[1]);
	node->parent_dir_block = 1;
	aux = &(node->fname[0]);
	strcpy((char*) aux, "Inside Carpeta");
	node->state = 2;
	node->file_size = 154234;

	node = &(node[1]);
	node->parent_dir_block = 2;
	aux = &(node->fname[0]);
	strcpy((char*) aux, "Inside Otra");
	node->state = 2;
	node->file_size = 154234;

	node = &(node[1]);
	node->parent_dir_block = 4;
	aux = &(node->fname[0]);
	strcpy((char*) aux, "Inside Otra Otra :D");
	node->state = 2;
	node->file_size = 154234;

	node = &(node[1]);
	node->parent_dir_block = 0;
	aux = &(node->fname[0]);
	strcpy((char*) aux, "Mi primer archivo.txt");
	node->state = 1;
	node->file_size = 10000;


	// Libera la memoria y cierra conexiones con el archivo.
	if (munmap(inicio, HEADER_SIZE_B + BITMAP_SIZE_B + NODE_TABLE_SIZE_B) == -1) printf("ERROR");
	close(fd);
}


/* @DESC
 * 		Determina cual es el nodo sobre el cual se encuentra un path.
 *
 * 	@PARAM
 * 		path - Direccion del directorio o archivo a buscar. No debe finalizar en '/'.
 * 		block_type - Define si se esta buscando un bloque borrado(0), un archivo(1) o un directorio(2).
 *
 * 	@RETURN
 * 		Devuelve el numero de bloque en el que se encuentra el nombre.
 * 		Si el nombre no se encuentra, devuelve -1.
 *
 */
ptrGBloque determinar_nodo(const char* path, int block_type){
	int fd, i;
	struct grasa_file_t *node, *inicio;
	unsigned char *node_name;	// Es el nombre obtenido del nodo que tengamos abierto.
	char *nombre = malloc(strlen(path)); // Es el nombre obtenido del Path que manda FUSE.
	char *start = nombre;
	// Si es el directorio raiz, devuelve 0:
	if(!strcmp(path, "/")) return 0;
	// Acomoda el nombre del archivo.
	strcpy(nombre, path);
	if (lastchar(path, '/')) {
		nombre[strlen(nombre)-1] = '\0';
	}
	nombre = strrchr(nombre, '/');
	nombre = &nombre[1]; // Acomoda el nombre, ya que el primer digito siempre es '/'

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
	for (i = 0; ( (strcmp(nombre, (char*) node_name) != 0) | (node->state != block_type)) &  (i < GFILEBYTABLE) ; i++ ){
		node = AVANZAR_BLOQUES(node,1);
		node_name = &(node->fname[0]);
	}

	// Cierra conexiones y libera memoria.
	free(start);
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
 */
static int grasa_getattr(const char *path, struct stat *stbuf) {

	memset(stbuf, 0, sizeof(struct stat));

	if (strcmp(path, "/") == 0) {
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
		return 0;
	}
//	if (determinar_nodo(path,DIRECTORY_T) >= 0){
//		stbuf->st_mode = S_IFDIR | 0755;
//		stbuf->st_nlink = 2;
//		return 0;
//	}
	if (determinar_nodo(path,FILE_T) >= 0){
		stbuf->st_mode = S_IFREG | 0444;
		stbuf->st_nlink = 1;
		stbuf->st_size = 100000;
		return 0;
	}
	return -ENOENT;
//
//	if (strcmp(path, "/") == 0) {
//		stbuf->st_mode = S_IFDIR | 0755;
//		stbuf->st_nlink = 2;
//		return 0;
//	}
//
//	stbuf->st_mode = S_IFREG | 0444;
//	stbuf->st_nlink = 1;
//	stbuf->st_size = 10000000;
//
//	return 0;
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
//static int grasa_readdir(const char *path){
	int fd, i, nodo = determinar_nodo((char*) path, DIRECTORY_T);
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
//		if ((nodo==(node->parent_dir_block)) & (((node->state) == 1) | ((node->state) == 2)))  printf("%s\n",(char*) &(node->fname[0]));
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
 */
//static int grasa_getattr(const char *path, struct stat *info){
//
//	return -ENOENT;
//
//}


/*
 * Esta es la estructura principal de FUSE con la cual nosotros le decimos a
 * biblioteca que funciones tiene que invocar segun que se le pida a FUSE.
 * Como se observa la estructura contiene punteros a funciones.
 */

static struct fuse_operations grasa_oper = {
		.readdir = grasa_readdir,
		.getattr = grasa_getattr,
};

/** keys for FUSE_OPT_ options */
enum {
	KEY_VERSION,
	KEY_HELP,
};


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

	escribir_uno();

//	 printf ("%d \n",strcmp("Inside Otra Otra :D", "Inside Otra Otra :D"));
//
//	char *nombre = "Inside Otra Otra :D";
//	unsigned char *node_name = (unsigned char*)"Inside Otra";
//	int i;
//
//
//	for (i = 0; ( (strcmp(nombre, (char*) node_name) != 0) | (0)) &  (i < 1) ; i++ ){
//			printf("ASD \n");
//		}
//
//	 printf ("%d \n",determinar_nodo("/Carpeta/Inside Otra/Inside Otra Otra :D", (DIRECTORY_T)));
//
//	 grasa_readdir("/Otra Carpetita/Inside Otra/inside Otra Otra :D");
//
//	 return 0;

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
	// de realizar el montaje, comuniscarse con el kernel, delegar todo
	// en varios threads
	return fuse_main(args.argc, args.argv, &grasa_oper, NULL);

}
