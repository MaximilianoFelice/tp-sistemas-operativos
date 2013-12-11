/*
 * Grasa_Dummy.c
 *
 *  Created on: 23/11/2013
 *      Author: utnso
 */

#include "grasa.h"

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
 * 		O archivo fue encontrado.
 * 		-EACCES archivo no es accesible.
 * 		-ENOENT archivo no encontrado.
 */
int grasa_open(const char *path, struct fuse_file_info *fi) {

	return 0;
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
int grasa_access(const char* path, int flags){

	return 0;
}

/*
 * 	@DESC
 * 		Modifica los permisos del archivo.
 * 		Como nosotros no trabajamos con permisos en el FS, esta funcion sera simplemente un hermoso Dummy.
 *
 * 	@PARAM
 * 		path - Ruta del archivo a cambiarle permisos
 * 		mode - Estructura que contiene los datos a cambiar del archivo
 *
 * 	@RETURN
 * 		0 - Funciona.
 * 		Negativo - Rompe.
 */
int grasa_chmod(const char *path, mode_t mode){

	return 0;
}

/*
 * 	@DESC
 * 		Modifica el owner y owner group del archivo.
 * 		Como nosotros no trabajamos con owners en el FS, esta funcion sera simplemente un hermoso Dummy.
 *
 * 	@PARAM
 * 		path - Ruta del archivo a cambiarle permisos
 *		user_data - Datos del usuario
 *		group_data - Datos del grupo
 *
 * 	@RETURN
 * 		0 - Funciona.
 * 		Negativo - Rompe.
 */
int grasa_chown(const char *path, uid_t user_data, gid_t group_data){

	return 0;
}
