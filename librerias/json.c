#include "string.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "json.h"



//NO OLVIDARSE DE LINKEAR A LAS COMMONS SINO NO VA A ANDAR.

/*
 * La libreria no es la mejor de todas pero nos va a ser muy util a la hora de pasar mensajes entre los dos distintos programas. Eso si, hay que tener alguanas cosas en cuenta:
 * Los valores no pueden tener ni ' ' " [ ] { }  , si alguien usa alguna de esas vamos a estar en problemas.
 * Ya se que parecen muchas funciones, pero si miran el funcionamiento es re simple, con el ejemplo van a entender todo. Sino miren el .h que es mas que simple.
 *
 */




/*
 * funciones de ejemplo
 *
 */
char * funcionMoverse(char * json){
	char * mover = "me movi";
	return mover;
}

char * morir(char * json){
	char * morir = "me mori";
	return morir;
}

char * conectar(char *json){
	char * conect = "me conecte";
	return conect;
}



void ejemplo(int argc, char **argv){

	//creo el json
	char* json = crearNuevoJson();

	// le agrego una accion, seria por ejemplo lo que le llega al planificador
	agregarPropiedad( "accion"		,"moverse"		, &json );
	agregarPropiedad( "coNoSer"		,"Aprobamos"	, &json );
	agregarPropiedad( "nombre"		,"nicolas"		, &json );
	//ejemplo del array, si una propiedad ya existe se transformara en array
	agregarPropiedad( "soyUnArray"	,"elemento1"	, &json );
	agregarPropiedad( "soyUnArray"	,"elemento2"	, &json );
	agregarPropiedad( "soyUnArray"	,"elemento3"	, &json );
	agregarPropiedad( "soyUnArray"	,"elemento4"	, &json );
	//ejemplo de como puedo guardar un numero como char * y despues devolverlo como int
	agregarPropiedad( "numeros"		,"5"			, &json );
	agregarPropiedad( "numeros"		,"85"			, &json );
	agregarPropiedad( "numeros"		,"45"			, &json );
	agregarPropiedad( "Ultima"		,"Valor"		, &json );


	printf("El json: %s\n\n", json);
	borrarPropiedad("coNoSer" , &json);
	printf("Ademas borro la propiedad c-O-No-ser y queda: %s\n\n", json);
	char * nombre =  devolverPropiedad("nombre",json);
	printf("El valor del nombre es: %s\n",nombre);
	//free(nombre);
	printf("La suma de los numeros es: %d\n",
			devolverPropiedadDelArrayComoInt 	( "numeros", 0 ,  json )+
			devolverPropiedadDelArrayComoInt 	( "numeros", 1 ,  json )+
			devolverPropiedadDelArrayComoInt 	( "numeros", 2 ,  json )
	);


	/*
	 * aca viene la parte importante es que cuando el mensaje le llegue por ejemplo al nivel o al personaje
	 * segun lo que diga en accion va a ejecutar una funcion distinta
	 */

	printf("\n\nAhora depentiendo de la variable accion voy a ejecutar alguna funcion que acepte y devuelva un char *\n");

	/*
	 * La idea es bindearle una accion al json segun lo que diga la accion. fijense que en el json acccion = moverse,
	 * por lo tanto va a ejecutar funcionMoverse, que devuelve la respuesta al mensaje que se le enviaria
	 */
	switch_accion( json , "moverse" 	, funcionMoverse);
	switch_accion( json , "morise" 		, morir);
	switch_accion( json , "conectar" 	, conectar);

	printf("La funcion respondio: %s", respuesta_Switch() );

	free(json);


}



















char * valor_respuesta_Switch;




/*
 * Ejecuta la funcion si la accion enviada es igual a la accion en el json.
 * La funcion debe aceptar y devolver una cadena de caracteres, se le enviara
 * el json y debera devolver una respuesta que se podra leer con respuesta_switch()
 *
 * Ej: switch_accion( {"accion":"moverse","posicionX":"5", "posicionY":"6" } , "moverse" , funcion)
 *
 */
void switch_accion( char * json , char * accion , char* (*funcion) (char * )){

	if( sonIguales_Strings (devolverPropiedad("accion" , 	json ), accion) > 0){
		valor_respuesta_Switch =  funcion(json);

	}

}


/*
 * Devuelve la accion ejecutada por la ultima accion
 *
 */
char * respuesta_Switch(){
	return valor_respuesta_Switch;
}



/*
 * Concatena array, reserva el espacio necesario. Al primero le agrega el segundo.
 * EXISTE LA FUNCION string_append EN LAS COMMONS PERO POR ALGUNA RAZON ME TIRA SEGMENTATION FAULT,
 * YO CUANDO HICE MI FUNCION AL PRINCIPIO LA HICE IGUAL. NO SE DE DONDE SALE EL ERROR.
 */
void 	string_concat( char ** valor1, char* valor2){

	int tamanio = strlen(*valor1) + strlen(valor2) + 1;
	char * nuevoString = malloc(tamanio);
	strcpy(nuevoString, *valor1);
	//free(*valor1);
	strcat(nuevoString, valor2);
	*valor1 = nuevoString;

}






/*
 * No es una funcion muy util pero sirve para que el codigo sea mas legible cuando se use.
 */
char * 	crearNuevoJson(){
	char * nuevoJson = "{}";
	return nuevoJson;
}

/*
 * Le agrega al json la propiedad que se le envia. En caso de ya existir, la transforma en un array y le mete el valor
 * @param propiedad: Es el nombre que se quiere para despues identificar
 * @param valor: Es el valor que se guarda
 * @param json: Es el json donde se va a guardar, con que sea un "{}" ya anda
 */

void  	agregarPropiedad ( char* propiedad , char* valor , char** json ){

	if(existePropiedad( propiedad,*json) == 0)
	{
		*json = string_substring_until( *json , strlen(*json) - 1);

		if (cantidadPropiedades(*json) > 0)
			string_concat(json, ",");

		string_concat(json, "'");
		string_concat(json, propiedad);
		string_concat(json, "':'");
		string_concat(json, valor);
		string_concat(json, "'}");
	}else{



		if(! existeArray( propiedad,  *json) ){
			char * valorAuxiliarDeLaPropiedad = (char * )devolverPropiedad( propiedad , *json);
			borrarPropiedad(propiedad , json);
			agregarPropArray ( propiedad ,  json);
			agregarValorArray ( propiedad, valorAuxiliarDeLaPropiedad, json);
			free(valorAuxiliarDeLaPropiedad);
		}
		agregarValorArray ( propiedad, valor, json);

	}



}


/*
 * Busca una cadena de caracteres dentro de otra, si la encuentra devuelve la posicion, sino -1
 * @param string: es la cadena en donde voy a buscar
 * @param buscado: es la cadena que voy a buscar
 */
int 	indexOf( char* string, char* buscado){
	int pos = strstr(string, buscado) - string;
	//printf("pos=%d\n",pos);
	if (pos>0)
		return pos;
	else
		return -1;
}



/*
 * Primero busca una subcadena cuando la encuentra. Devuelve todo el texto anterior. En caso de no encontrar la cadena, devuelve el mismo texto
 * @param texto: una cadena en la que se va a buscar.
 * @param buscado: una subcadena que se va a buscar
 * Ejemlo: substring_HastaEncontrar("hola como estas?","como") deberia devolver "hola"
 */
char * 	substring_HastaEncontrar( char * texto, char * buscado){
	int pos = indexOf( texto , buscado );
	if (pos != -1)
		return string_substring_until( texto ,  pos);
	else
		return texto;
}



/*
 * A diferencia de substring_HastaEncontrar cuando encuentra el texto, desde ahi empieza a substringuear
 *
 */
char * 	substring_DesdeEncontrar( char * texto, char * buscado){
	int pos = indexOf( texto , buscado );
		if (pos != -1)
			return string_substring_from( texto , pos );
		else
			return texto;

}



/*
 * Le agregar un valor a un array que sea propiedad, lo pushea. Es una pila. Primero tiene que existir el array, crearlo con agregarPropArray
 * @param propiedad: nombre del array
 * @param valor: es el valor a guardar
 * @param json: es el json donde se va a guardar
 */
void 	agregarValorArray ( char* propiedad, char * valor, char **json){

	char * nuevaCadena = "";
	char * jsonHastaArray = "";
	char * jsonDesdeArray = "";

	string_concat(&jsonHastaArray , "'");
	string_concat(&jsonHastaArray , propiedad);
	string_concat(&jsonHastaArray , "':[");

	int posAAgregar = indexOf( *json , jsonHastaArray);
	posAAgregar += 4 + strlen(propiedad);
	//printf("%d", posAAgregar);
	jsonDesdeArray = string_substring(*json , 0 ,posAAgregar  );
	jsonHastaArray = string_substring(*json , posAAgregar , strlen(*json) );


	string_concat(&nuevaCadena , jsonDesdeArray);
	string_concat(&nuevaCadena , valor);

	if(cantidadElementosEnArray( propiedad,*json) > 0 ){
		string_concat(&nuevaCadena , ",");
	}

	string_concat(&nuevaCadena , jsonHastaArray);
	*json = nuevaCadena;
	//free(propiedad);
	//free(valor);
	free(jsonHastaArray);
	free(jsonDesdeArray);

}





/*
 * Devuelve la cantidad de elementos que hay en una propiedad que sea array
 * @param propiedad: nombre del array
 * @param array: es el json en donde se buscara
 *
 */
int 	cantidadElementosEnArray( char* propiedad, char * json){
	int cantidad = 1;
	char * cadenaBuscada = "";
	string_concat(&cadenaBuscada , "'");
	string_concat(&cadenaBuscada , propiedad);
	string_concat(&cadenaBuscada , "'");
	char * cadenaRestante = substring_DesdeEncontrar( json , cadenaBuscada);
	cadenaRestante = substring_HastaEncontrar(cadenaRestante ,"]");

	if( strlen(cadenaRestante) == strlen(propiedad) + 4 )
		return 0;

	for( int i = 0; i< strlen(cadenaRestante); i++)
	{
		if(cadenaRestante[i] == ',')
			cantidad++;
	}
	return cantidad;
}




/*
 * Agrega un array/propiedad vacio para despues agregarle elementos
 * @param propiedad: es el nombre que se va a agregar
 * @param json: es el json en donde se agregara el array
 *
 */
void  	agregarPropArray ( char* propiedad ,  char ** json){

	*json = string_substring_until( *json , strlen(*json) - 1);

	if (cantidadPropiedades(*json) > 0)
		string_concat(json, ",");

	string_concat(json, "'");
	string_concat(json, propiedad);
	string_concat(json, "':[]}");

}



/*
 * Devuelve la cantidad de propiedades del json, los arrays los cuenta como 1 solo
 * @param json: el json a contar
 *
 */
int   	cantidadPropiedades(char * json){
	int cant = 0;
	for( int i = 0; i< strlen(json); i++)
	{
		if(json[i] == ':')
			cant++;
	}
	return cant;
}


/*
 * Devuelve el valor almacenado en una propiedad
 * @param propiedad: Es el nombre a buscar
 * @param json: Es el json a buscar
 *
 */
char * 	devolverPropiedad(char * propiedad , char * json)
{
	char * inicioPropiedadValor = strstr(json , propiedad);
	if(inicioPropiedadValor == NULL)
		return "Error";
	json = string_substring_from(json,inicioPropiedadValor - json);

	char * posProxElemento = strstr(json, ",");
	if (posProxElemento == NULL)
		posProxElemento = strstr(json, "}");
	json = string_substring_until(json , posProxElemento - json -1 );
	char * inicioPropiedad = strstr(json , ":");
	json = string_substring_from(json, inicioPropiedad - json +2);

	//free(inicioPropiedad);
	//free(inicioPropiedadValor);
	//free(posProxElemento);

	return json;
}



/*
 * Compara si 2 strings son iguales. Devuelve 1 si lo son o 0 si no
 * @param string1: string a comprar
 * @param string2: string a comprar
 *
 */
int   	sonIguales_Strings( char * string1 , char * string2){

	if(strlen(string1) != strlen(string2) )
		return 0;

	if ( strncmp(string1,string2 , strlen(string1))  == 0)
		return 1;
	else
		return 0;
}



/*
 * Elimina una propiedad. NO USAR CON ARRAYS. (va a ser algo feo)
 * @param propiedad: propiedad a borrar
 *
 */
void 	borrarPropiedad(char * propiedad , char ** json) {

	if(existePropiedad(propiedad, *json) == 0){
		printf("Error en la funcion borrar propiedad, se mando una propiedad inexistente");
		return;
	}

	char * cadenaInicial = "";
	char * cadenaRestante = "";
	int posInicioPropiedad = indexOf(*json, propiedad );
	cadenaInicial = string_substring(*json, 0, posInicioPropiedad -1);
	cadenaRestante = string_substring(*json, posInicioPropiedad, strlen(*json) );

	if( indexOf(cadenaRestante, ",") != -1){
		//No es el ultimo elemento
		cadenaRestante = substring_DesdeEncontrar(cadenaRestante ,",");
		cadenaRestante = string_substring(cadenaRestante, 1 , strlen(cadenaRestante));
	}else{
		//Es el ultimo elemento
		cadenaInicial = string_substring(cadenaInicial, 0, strlen(cadenaInicial) -1);
		cadenaRestante = "}";
	}

	//printf("cadenaInicial: %s \ncadena final: %s\n\n" , cadenaInicial, cadenaRestante);
	string_concat(&cadenaInicial, cadenaRestante);
	*json = cadenaInicial;
	//free(cadenaRestante);

}


/*
 * Devuelve el valor de un elemento de un array en la posicion deseada.
 * @param propiedad: es el array/propiedad a buscar
 * @param posicion: es el indice del elemento a buscar. Empezando desde 0
 * @param json: es el json a buscar
 * Ejemplo: devolverPropiedadDelArray("array", 2,"{"array":[elemento1,elemento2,elemento3,elemento4]}") 	=> elemento3
 *
 */
char * 	devolverPropiedadDelArray ( char * propiedad , int posicion , char * json){

	if( posicion>cantidadElementosEnArray( propiedad,  json) ){
		printf("Error en la funcion devolverPropiedadDelArray, se envio un indice fuera de rango");
		return "";
	}

	char * cadenaBuscada = "";
	string_concat( &cadenaBuscada , "'");
	string_concat( &cadenaBuscada , propiedad);
	string_concat( &cadenaBuscada , "':[");

	char * cadenaRestante = substring_DesdeEncontrar( json , cadenaBuscada);
	cadenaRestante = substring_HastaEncontrar(cadenaRestante, "]");
	cadenaRestante = string_substring(cadenaRestante, 4 + strlen(propiedad) , strlen(cadenaRestante));
	//printf("El array tiene %d elementos \n",cantidadElementosEnArray( propiedad,  json) );

	for (int i = 0 ; i< cantidadElementosEnArray( propiedad,  json) && i < posicion ; i++){
		cadenaRestante = substring_DesdeEncontrar(cadenaRestante, ",");
		cadenaRestante = string_substring(cadenaRestante, 1 , strlen(cadenaRestante));
		//printf("%s \n" , cadenaRestante);
	}

	if((posicion +1) ==  cantidadElementosEnArray( propiedad,  json) )
	{
		return cadenaRestante;
	}else{
		int hasta = indexOf( cadenaRestante , ",");
		return string_substring( cadenaRestante, 0 ,hasta );
	}

	free(cadenaBuscada);
}


/*
 * Devuelve una propiedad como int.
 *
 */
int		devolverPropiedadComoInt( char * propiedad , char * json){
	char * numero = devolverPropiedad(propiedad , json);
	int num = atoi(numero);
	free(numero);
	return num;
}


/*
 * Devuelve el valor de un elemento de un array en una posicion como int
 *
 */
int 	devolverPropiedadDelArrayComoInt ( char * propiedad , int posicion , char * json){
	char * numero = devolverPropiedadDelArray(propiedad, posicion, json);
	int num = atoi(numero);
	free(numero);
	return num;
}

/*
 * Devuelve si existe una propiedad o uno. Sea array o no.
 * 1 Si existe
 * 0 si NO existe
 *
 */
int		existePropiedad( char * propiedad,char * json){

	char * cadenaABuscar = "";
	string_concat(&cadenaABuscar, "'");
	string_concat(&cadenaABuscar, propiedad);
	string_concat(&cadenaABuscar, "':");

	if (indexOf(json, cadenaABuscar) != -1){
		free(cadenaABuscar);
		return 1;
	}
	free(cadenaABuscar);
	return 0;
}


/*
 * Devuelve si existe un array, parecido al anterior pero mas estricto.
 * Solo devuelve 1 existe y es un array.
 * Devuelve 0 no existe o existe pero no es array
 *
 */
int		existeArray( char * nombreArray, char * json)
{
	char * cadenaABuscar = "";
	string_concat(&cadenaABuscar, "'");
	string_concat(&cadenaABuscar, nombreArray);
	string_concat(&cadenaABuscar, "':[");

	if (indexOf(json, cadenaABuscar) != -1){
		free(cadenaABuscar);
		return 1;
	}
	free(cadenaABuscar);
	return 0;
}













