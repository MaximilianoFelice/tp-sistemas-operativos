/*
 * FileSystem.c
 *
 *  Created on: 28/09/2013
 *      Author: Maximiliano Felice
 */
#include "grasa.h"
//#include "Grasa_Handlers.c"



struct t_runtime_options {
	char* welcome_msg;
	char* define_disc_path;
	char* log_level_param;
	char* log_path_param;
} runtime_options;

/*
 * Esta es la estructura principal de FUSE con la cual nosotros le decimos a
 * biblioteca que funciones tiene que invocar segun que se le pida a FUSE.
 * Como se observa la estructura contiene punteros a funciones.
 */
/* Es valido aclarar que FUSE necesita las operaciones de Truncate, Chown, Chmod y Utime para realizar su
 * operacion SETATTR. Se puede remplazar, dependiendo de la implementacion, por SETXATTR, que depende de
 * un define realizado en el versionado de FUSE.
 * En GRASA, como trabajamos con FUSE 2.2, no es explicitamente necesario SETXATTR, pero si debemos tener
 * definidas las operaciones previamente definidas.
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
		.rename = grasa_rename,		// OK
		.setxattr = grasa_setxattr,	// OK
		.access = grasa_access,		// OK
		.chmod = grasa_chmod,		// OK
		.utime = grasa_utime,		// OK
		.chown = grasa_chown,		// OK
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
	return 0;
}

void sig_int_handler(int sig){
	log_info(logger, "Recibido signal SIGUSR1");
	if (sig == SIGUSR1) {
		printf("\n%d\n", obtain_free_blocks());
	}
	log_info(logger, "SIGUSR1 res: %d", bitmap_free_blocks);
}

void sig_term_handler(int sig){
	log_error(logger, "Programa terminado anormalmente con signal %d", sig);
	// Termina el programa de forma normal.
	if (sig == SIGTERM){

		fdatasync(discDescriptor);

		// Destruye el lock:
			pthread_rwlock_destroy(&rwlock);

			// Destruye el log
			log_destroy(logger);

			// Cierra lo que tiene en memoria.
			munlockall(); /* Desbloquea todas las paginas que tenia bloqueadas */
			if (munmap(header_start, ACTUAL_DISC_SIZE_B ) == -1) printf("ERROR");

			close(discDescriptor);
	}
}

int main (int argc, char *argv[]){

	DISABLE_DELETE_MODE;

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
		printf("Mountpoint not specified: Unloading modules.");
		sig_term_handler(SIGTERM);
	}

	// Settea el log level del disco:
	t_log_level log_level = LOG_LEVEL_NONE;
	if (runtime_options.log_level_param != NULL){
		if (!strcmp(runtime_options.log_level_param, "LockTrace")) log_level = LOG_LEVEL_LOCK_TRACE;
		else if (!strcmp(runtime_options.log_level_param, "Trace")) log_level = LOG_LEVEL_TRACE;
		else if (!strcmp(runtime_options.log_level_param, "Debug")) log_level = LOG_LEVEL_DEBUG;
		else if (!strcmp(runtime_options.log_level_param, "Info")) log_level = LOG_LEVEL_INFO;
		else if (!strcmp(runtime_options.log_level_param, "Warning")) log_level = LOG_LEVEL_WARNING;
		else if (!strcmp(runtime_options.log_level_param, "Error")) log_level = LOG_LEVEL_ERROR;
		else if (!strcmp(runtime_options.log_level_param, "None")) log_level = LOG_LEVEL_NONE;
		else log_level = LOG_LEVEL_NONE;
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
	/* Obliga a que se mantenga la tabla de nodos y el bitmap en memoria */
	mlock(bitmap_start, BITMAP_BLOCK_SIZE*BLOCKSIZE);
	mlock(node_table_start, NODE_TABLE_SIZE*BLOCKSIZE);
	/* El codigo es tan, pero tan egocentrico, que le dice al SO como tratar la memoria */
	madvise(header_start, ACTUAL_DISC_SIZE_B ,MADV_RANDOM);

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

	fdatasync(discDescriptor);

	// Destruye el lock:
	pthread_rwlock_destroy(&rwlock);
	pthread_rwlockattr_destroy(&attrib);

	// Destruye el log
	log_destroy(logger);

	// Cierra lo que tiene en memoria.
	munlockall(); /* Desbloquea todas las paginas que tenia bloqueadas */
	if (munmap(header_start, ACTUAL_DISC_SIZE_B ) == -1) printf("ERROR");

	close(fd);

	return res;

}
