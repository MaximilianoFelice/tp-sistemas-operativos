
/*
 * Sistemas Operativos - Super Mario Proc RELOADED.
 * Grupo       : C o no ser.
 * Nombre      : plataforma.c.
 * Descripcion : Contiene los ENUM's utilizados en el TP con la funcionalidad de obtener el string del mismo
 *
 * Muchas veces imprimimos por pantalla "El msj (enum con tipoMensaje )no es correcto" y mostramos el int del enum que llegó
 * Como no es cómodo hacer eso ya que debemos revisar que numero es de enum armé esta biblioteca que
 * devuelve el string de cada enum
 *
 */

#include "enumToString.h"

static _Bool _existEnum(int i);

static const char *ENUM_TO_STRING[] = {
    FOREACH_TYPEMSJ(GENERATE_STRING)
};

/*
 * @NAME: printAllEnums
 * @DESC: imprime por pantalla el string de todos los enum's generados
 * @PARAMS: no recibe nada
 */
void printAllEnums(){
	int i=0;
	while(_existEnum(i)){
		printf("The enum string of %d is %s\n", i, enumToString(i));
		i++;
	}
	printf("Fin de impresion de enums\n");
}

/*
 * @NAME: enumToString
 * @DESC: retorna el string de un enum si existe, si no devuelve NULL
 * @PARAMS: recibe el numero de un enum
 */
const char * enumToString(int enumIndex){
	if(_existEnum(enumIndex))
		return ENUM_TO_STRING[enumIndex];
	else
		return NULL;
}

/*
 * @NAME: enumSize
 * @DESC: retorna la cantidad total de valores del enum
 * @PARAMS: no recibe nada
 */
int enumSize(){
	int i=0;
	while(_existEnum(i))
		i++;
	return i;
}

//Funciones privadas

/*
 * @NAME: existEnum
 * @DESC: funcion privada que retorna true si existe el enum indicado
 * @PARAMS: recibe el numero de un enum
 */
static _Bool _existEnum(int i){
	return (ENUM_TO_STRING[i] != NULL);
}


////Ejemplo de funcionamiento: para probarlo poner el .c y .h en un proyecto aparte
/*
int main(){
	printAllEnums();
	tMensaje tipoMsj = P_HANDSHAKE;
	printf("\nThe enum string of %d is %s\n", tipoMsj, enumToString(tipoMsj));
	printf("\nLa cantidad de enums es %d", enumSize());
	return EXIT_SUCCESS;
}
*/


