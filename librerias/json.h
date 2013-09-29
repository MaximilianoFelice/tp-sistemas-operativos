#include "string.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>


char* 	crearNuevoJson						();
char* 	respuesta_Switch					();
void 	switch_accion						( char * json , char * accion , char* (*funcion) (char * ));

void  	agregarPropiedad 					( char*  propiedad , 	char*  	valor, char** json);
void  	agregarPropArray 					( char*  propiedad ,	char** 	json);
void 	agregarValorArray 					( char*  propiedad, 	char*  	valor, char **json);
void   	borrarPropiedad						( char*  propiedad , 	char** 	json);

int   	sonIguales_Strings					( char*  string1 ,		char*  	string2);
void 	string_concat						( char** valor1,		char*  	valor2);
int 	indexOf								( char*  string, 		char*  	buscado);
char* 	substring_HastaEncontrar			( char*  texto, 		char*  	buscado);
char* 	substring_DesdeEncontrar			( char*  texto, 		char*  	buscado);

char* 	devolverPropiedad					( char*  propiedad , 	char*  	json);
char*	devolverPropiedadDelArray 			( char*  propiedad , 	int 	posicion , char * json);
int		devolverPropiedadComoInt			( char*  propiedad , 	char* 	json);
int 	devolverPropiedadDelArrayComoInt 	( char * propiedad, 	int 	posicion , char * json);

int		existePropiedad						( char * propiedad,		char* 	json);
int		existeArray							( char * nombreArray, 	char*	json);
int 	cantidadElementosEnArray 			( char*  propiedad, 	char*  	json);
int   	cantidadPropiedades					( char * json);
