/*
 * Sistemas Operativos - Super Mario Proc RELOADED.
 * Grupo       : C o no ser.
 * Nombre      : personaje.c.
 * Descripcion : Este archivo contiene la implementacion de las
 * funciones usadas por el personaje.
 */

#include "personaje.h"
#include <semaphore.h>

pthread_mutex_t semModificadorDeVidas; //Los semaforos solo para las variables globales
sem_t semReinicio;
sem_t semFinPlan;
bool reiniciar = false;

personajeGlobal_t personaje;
t_config *configPersonaje;
t_log *logger;
char * ip_plataforma;
char * puerto_orq;

int socketOrquestador;

int main(int argc, char*argv[]) {
	int cantidadNiveles;

	pthread_mutex_init(&semModificadorDeVidas, NULL);
	sem_init(&semReinicio, 0, 0);
	sem_init(&semFinPlan, 0, 0);

	// Inicializa el log.
	logger = logInit(argv, "PERSONAJE");

	if (signal(SIGINT, muertoPorSenial) == SIG_ERR) {
		log_error(logger, "Error en el manejo de la senal de muerte del personaje.\n", stderr);
		exit(EXIT_FAILURE);
	}

	if (signal(SIGTERM, restarVida) == SIG_ERR) {
		log_error(logger, "Error en el manejo de la senal de restar vidas del personaje.\n", stderr);
		exit(EXIT_FAILURE);
	}

	if (signal(SIGUSR1, sig_aumentar_vidas) == SIG_ERR) {
		log_error(logger, "Error en el manejo de la senal de de aumentar vidas del personaje.\n", stderr);
		exit(EXIT_FAILURE);
	}

	// Creamos el archivo de Configuración
	cargarArchivoConfiguracion(argv[1]);
	cantidadNiveles = list_size(personaje.lPersonajes);
	//Me conecto con la plataforma para despues de terminar todos los niveles correctamente avisarle
	socketOrquestador = connectToServer(ip_plataforma, atoi(puerto_orq), logger);
	log_debug(logger, "El personaje se conecto con el orquestador");

	//vuelve a tirar todos los hilos por todos los niveles
	int iNivel;
	char *resultadoThread;

	do {
		log_debug(logger, "Tiro los hilos para jugar en cada nivel");

		reiniciar = false;

		crearTodosLosHilos();

		//FIXME Ver cuando termina el plan de niveles -- hipoteticamente solucionado :D
		sem_wait(&semReinicio);
		sem_wait(&semFinPlan);
		//Ya terminaron todos los hilos. Si murio el ultimo me va a decir que reinicie o no.
		if (reiniciar) log_debug(logger, "El personaje %c termina su conexion y reinicia todos los niveles", personaje.simbolo);

	} while (reiniciar);

	cerrarConexion(&socketOrquestador);
	log_debug(logger, "El personaje se desconecto del orquestador");

	log_destroy(logger);
	config_destroy(configPersonaje);
	int cantidadPersonajes = list_size(personaje.lPersonajes);

	for (iNivel = 0; iNivel < cantidadPersonajes; iNivel++) {
		personajeIndividual_t *pPersonajeNivel = (personajeIndividual_t *) list_get(personaje.lPersonajes, iNivel);
		list_destroy(pPersonajeNivel->Objetivos);
		free(pPersonajeNivel->thread);
	}

	list_destroy(personaje.lPersonajes);

	exit(EXIT_SUCCESS);

}

bool finalizoPlan(){

	bool _finalizoPlan(personajeIndividual_t* personajePorNivel){
		return personajePorNivel->bienTerminado;
	}

	if (list_all_satisfy(personaje.lPersonajes, (void*) _finalizoPlan)) return true;
	return false;
}

void notificarFinPlanNiveles(){
	tPaquete pkgDevolverRecursos;
	pkgDevolverRecursos.type   = P_FIN_PLAN_NIVELES;
	pkgDevolverRecursos.length = 0;

	enviarPaquete(socketOrquestador, &pkgDevolverRecursos, logger, "Se notifica al orquestador la finalizacion del plan de niveles del personaje correctamente");
}

void cargarArchivoConfiguracion(char* archivoConfiguracion){
	personajeIndividual_t *personajeNivel;
	//valida que los campos basicos esten en el archivo
	configPersonaje  	   = config_try_create(archivoConfiguracion, "nombre,simbolo,Plan de Niveles,Vidas,orquestador");
	// Obtenemos el nombre del personaje - global de solo lectura
	personaje.nombre 	   = config_get_string_value(configPersonaje, "nombre");
	// Obtenemos el simbolo - global de solo lectura
	personaje.simbolo      = config_get_string_value(configPersonaje, "simbolo")[0];
	personaje.vidasMaximas = config_get_int_value(configPersonaje, "Vidas"); //Obtenemos las vidas
	personaje.vidas 	   = personaje.vidasMaximas;
	personaje.reintentos   = 0;
	// Obetenemos los datos del orquestador
	char * dir_orq = config_get_string_value(configPersonaje, "orquestador");
	obtenerIpYPuerto(dir_orq, ip_plataforma, puerto_orq);

	//Obtenemos el plan de niveles
	char** niveles = config_try_get_array_value(configPersonaje, "Plan de Niveles");
	t_list* listaObjetivos;
	personaje.lPersonajes = list_create();
	int nroNivel;

	//Armamos lista de niveles con sus listas de objetivos del config
	for (nroNivel = 0; niveles[nroNivel] != NULL; nroNivel++) {  //Cicla los niveles
		char* stringABuscar = string_from_format("Obj[%s]", niveles[nroNivel]);
		char** objetivos = config_try_get_array_value(configPersonaje, stringABuscar);
		free(stringABuscar);
		int nroObjetivo;
		//Por cada nivel, genero una lista de objetivos
		listaObjetivos = list_create();
		for (nroObjetivo = 0; objetivos[nroObjetivo] != NULL; nroObjetivo++) {
			list_add_new(listaObjetivos, objetivos[nroObjetivo], sizeof(char)); //Agrego a la lista
		}
		personajeNivel = (personajeIndividual_t*) malloc(sizeof(personajeIndividual_t));
		personajeNivel->nomNivel = string_duplicate(niveles[nroNivel]);
//		strcpy(personajeNivel->nomNivel, niveles[nroNivel]);
		personajeNivel->Objetivos = listaObjetivos;
		list_add(personaje.lPersonajes, personajeNivel);
	}

//	free(stringABuscar);
}

void crearTodosLosHilos() {
	int iIndexPj;
	int cantidadPersonajes = list_size(personaje.lPersonajes);
	for (iIndexPj = 0;  iIndexPj < cantidadPersonajes; iIndexPj ++) {
		personajeIndividual_t *personajeNivel = (personajeIndividual_t *)list_get(personaje.lPersonajes, iIndexPj);
		personajeNivel->thread = malloc(sizeof(pthread_t));

		//Tiro el hilo para jugar de cada nivel
		if (pthread_create(personajeNivel->thread, NULL, jugar, (void *) personajeNivel)) {
			log_error(logger, "pthread_create: %s", strerror(errno));
			exit(EXIT_FAILURE);
		}
	}


}

void obtenerIpYPuerto(char *dirADividir, char * ip,  char * puerto){
	ip_plataforma  = strtok(dirADividir, ":"); // Separar ip
	puerto_orq 	   = strtok(NULL, ":"); // Separar puerto
}

void *jugar(void *vPersonajeNivel) {

	personajeIndividual_t* pPersonajePorNivel = (personajeIndividual_t *)vPersonajeNivel;
	pPersonajePorNivel->posX = 0;
	pPersonajePorNivel->posY = 0;
	pPersonajePorNivel->posRecursoX=0;
	pPersonajePorNivel->posRecursoY=0;
	pPersonajePorNivel->recursoActual=0;
	pPersonajePorNivel->socketPlataforma = -1;
	pPersonajePorNivel->ultimoMovimiento = vacio;
	pPersonajePorNivel->murioEnNivel 	 = false;

	pPersonajePorNivel->bienTerminado = false;

	while (personaje.vidas>0 && !pPersonajePorNivel->bienTerminado) {

		pPersonajePorNivel->socketPlataforma = connectToServer(ip_plataforma, atoi(puerto_orq), logger);
		handshake_plataforma(pPersonajePorNivel);

		log_info(logger, "Vidas de %c: %d", personaje.simbolo, personaje.vidas);

		for (pPersonajePorNivel->recursoActual=0; (pPersonajePorNivel->recursoActual < list_size(pPersonajePorNivel->Objetivos)) && (!personajeEstaMuerto(pPersonajePorNivel->murioEnNivel)); pPersonajePorNivel->recursoActual++) {

			//Espera que el planificador le de el turno para pedir la posicion del recurso
			recibirMensajeTurno(pPersonajePorNivel);

			if (!personajeEstaMuerto(pPersonajePorNivel->murioEnNivel)) {

				//agarra un recurso de la lista de objetivos del nivel
				char* recurso = (char*) list_get(pPersonajePorNivel->Objetivos, pPersonajePorNivel->recursoActual);
				pedirPosicionRecurso(pPersonajePorNivel, recurso);

				while (!conseguiRecurso(pPersonajePorNivel) && !personajeEstaMuerto(pPersonajePorNivel->murioEnNivel)) {

					//Espera que el planificador le de el turno para moverse
					recibirMensajeTurno(pPersonajePorNivel);

					//El personaje se mueve
					moverAlPersonaje(pPersonajePorNivel);

				}

				if (!personajeEstaMuerto(pPersonajePorNivel->murioEnNivel)) {
					//Espera que el planificador le de el turno para solicitar el recurso
					recibirMensajeTurno(pPersonajePorNivel);
					solicitarRecurso(pPersonajePorNivel, recurso);
				}

			}
		} //Fin de for de objetivos

		if (pPersonajePorNivel->murioEnNivel) {
			pthread_mutex_lock(&semModificadorDeVidas);

			personaje.vidas--;
			log_debug(logger, "El personaje %c perdio una vida en %s", personaje.simbolo, pPersonajePorNivel->nomNivel);
			if (personaje.vidas > 0) {
				pPersonajePorNivel->bienTerminado = false;
				pPersonajePorNivel->murioEnNivel = false;
				desconectarPersonaje(pPersonajePorNivel);
				pthread_mutex_unlock(&semModificadorDeVidas);
			} else {
				log_debug(logger, "El personaje %c detecto que no hay mas vidas\n", personaje.simbolo);
				matarHilosExceptoYo(pPersonajePorNivel->nomNivel); //Mato a los otros hilos
				desconectarPersonaje(pPersonajePorNivel);
				reiniciar = consultarReinicio(); //Aqui es donde me asesino
				pthread_mutex_unlock(&semModificadorDeVidas);
				if (reiniciar) {
					sem_post(&semReinicio); //SIGNAL(sem)
					sem_post(&semFinPlan);
					pthread_exit(NULL);
				}
			}

		}
		else {
			pPersonajePorNivel->bienTerminado = true; //Ya termino este nivel
			log_debug(logger, "El personaje termino bien el nivel %s", pPersonajePorNivel->nomNivel);

		}

	}//Fin del while

	desconectarPersonaje(pPersonajePorNivel);
	log_debug(logger, "El personaje se desconecto de la plataforma");
	sem_post(&semReinicio);
	if (finalizoPlan())	sem_post(&semFinPlan);
	char * exit_return = strdup("El personaje ha finalizado su plan de nivel");
	pthread_exit((void *)exit_return);
}

void matarHilosExceptoYo(char *nombreNivel) {
	int cantidadNiveles, indexNivel;
	cantidadNiveles = list_size(personaje.lPersonajes);

    for (indexNivel = 0; indexNivel < cantidadNiveles; indexNivel++) {
    	personajeIndividual_t *unPersonajeNivel = list_get(personaje.lPersonajes, indexNivel);
    	if (strcmp(unPersonajeNivel->nomNivel, nombreNivel) != 0) { //No me quiero asesinar! Aún.
    		log_debug(logger, "Soy el hilo del %s y voy a matar a %s", nombreNivel, unPersonajeNivel->nomNivel);
    		pthread_cancel(*unPersonajeNivel->thread); //Le manda señal SIGILL a cada hilo
    		close(unPersonajeNivel->socketPlataforma);
    	}
    }
}


bool personajeEstaMuerto(bool murioPersonaje){
	//si esta muerto por alguna señal o porque se quedo sin vidas
	return (murioPersonaje || personaje.vidas<=0);
}

void desconectarPersonaje(personajeIndividual_t* personajePorNivel){
	close(personajePorNivel->socketPlataforma);
	personajePorNivel->socketPlataforma = -1;
}

bool conseguiRecurso(personajeIndividual_t *personajePorNivel) {
	return ((personajePorNivel->posY == personajePorNivel->posRecursoY) && (personajePorNivel->posX == personajePorNivel->posRecursoX));
}

void moverAlPersonaje(personajeIndividual_t* personajePorNivel){
	tDirMovimiento  mov;

	mov = calcularYEnviarMovimiento(personajePorNivel);
	//Actualizo mi posición y de acuerdo a eso armo mensaje de TURNO

	actualizaPosicion(&mov, personajePorNivel);
}

void solicitarRecurso(personajeIndividual_t* personajePorNivel, char *recurso){

	tMensaje tipoMensaje;
	tPregPosicion recursoSolicitado;
	recursoSolicitado.simbolo=personaje.simbolo;
	recursoSolicitado.recurso=*recurso;

	tPaquete pkgSolicitudRecurso;
	serializarPregPosicion(P_SOLICITUD_RECURSO, recursoSolicitado, &pkgSolicitudRecurso);

	char *msjInfo = malloc(100); //Lo agrego para saber que hilo de cual nivel hizo la solicitud
	sprintf(msjInfo, "%s: El personaje le envia la solicitud del recurso a la plataforma", personajePorNivel->nomNivel);

	enviarPaquete(personajePorNivel->socketPlataforma, &pkgSolicitudRecurso, logger, msjInfo);

	char* sPayload;
	sprintf(msjInfo, "%s: El personaje recibe respuesta de la solicitud del recurso a la plataforma", personajePorNivel->nomNivel);
	int response = recibirPaquete(personajePorNivel->socketPlataforma, &tipoMensaje, &sPayload, logger, msjInfo);

	if (response <= 0) {
		log_info(logger, "Se murio la plataforma, CHAUUUUUUUUUUUUUUUUUU!");
		exit(EXIT_FAILURE);
	}

	free(msjInfo);

	switch(tipoMensaje){
		case PL_RECURSO_OTORGADO:{
			log_info(logger, "%s: El personaje recibe el recurso", personajePorNivel->nomNivel);
			break;
		}
		case PL_RECURSO_INEXISTENTE:{
			log_error(logger, "%s: El recurso pedido por el personaje no existe.", personajePorNivel->nomNivel);
			exit(EXIT_FAILURE);
			break;
		}
		case PL_MUERTO_POR_ENEMIGO:{
			log_info(logger, "%s: El personaje se murio por enemigos", personajePorNivel->nomNivel);
			personajePorNivel->murioEnNivel=true;
			break;
		}
		case PL_MUERTO_POR_DEADLOCK:{
			log_info(logger, "%s: El personaje se murio por deadlock", personajePorNivel->nomNivel);
			personajePorNivel->murioEnNivel=true;
			break;
		}
		default: {
			log_error(logger, "%s: Llego un mensaje (tipoMensaje: %s) cuando debia llegar PL_SOLICITUD_RECURSO", personajePorNivel->nomNivel, enumToString(tipoMensaje));
			exit(EXIT_FAILURE);
			break;
		}
	}

}


tDirMovimiento calcularYEnviarMovimiento(personajeIndividual_t *personajePorNivel){
	tMensaje tipoMensaje;
	tMovimientoPers movimientoAEnviar;


	movimientoAEnviar.simbolo=personaje.simbolo;

	log_debug(logger, "Se calcula el movimiento a realizar.");
	calculaMovimiento(personajePorNivel);
	movimientoAEnviar.direccion = (*personajePorNivel).ultimoMovimiento;
	log_debug(logger, "Movimiento calculado: personaje %c en (%d, %d)", personaje.simbolo, personajePorNivel->posX, personajePorNivel->posY);

	tPaquete pkgMovimiento;
	serializarMovimientoPers(P_MOVIMIENTO, movimientoAEnviar, &pkgMovimiento);

	char *msjInfo = malloc(100); //Lo agrego para saber que hilo de cual nivel hizo la solicitud
	sprintf(msjInfo, "%s: Envio pedido de movimiento del personaje", personajePorNivel->nomNivel);
	enviarPaquete(personajePorNivel->socketPlataforma, &pkgMovimiento, logger, msjInfo);

	char* sPayload;
	sprintf(msjInfo, "%s: Se espera confirmacion del movimiento", personajePorNivel->nomNivel);
	int response = recibirPaquete(personajePorNivel->socketPlataforma, &tipoMensaje, &sPayload, logger, "Se espera confirmacion del movimiento");

	if (response <= 0) {
		log_info(logger, "Se murio la plataforma, CHAUUUUUUUUUUUUUUUUUU!");
		exit(EXIT_FAILURE);
	}

	free(msjInfo);

	switch(tipoMensaje){
		case PL_MUERTO_POR_ENEMIGO:
			log_info(logger, "%s: El personaje se murio por enemigos mientra que calculaba movimiento", personajePorNivel->nomNivel);
			personajePorNivel->murioEnNivel=true;
			break;

		case PL_MUERTO_POR_DEADLOCK:
			log_info(logger, "%s: El personaje se murio por deadlock", personajePorNivel->nomNivel);
			personajePorNivel->murioEnNivel=true;
			break;

		case PL_CONFIRMACION_MOV:
			log_info(logger, "%s: Movimiento confirmado", personajePorNivel->nomNivel);
			break;

		default:
			log_error(logger, "%s: Llego un mensaje (tipoMensaje: %s) cuando debia llegar PL_CONFIRMACION_MOV", personajePorNivel->nomNivel, enumToString(tipoMensaje));
			exit(EXIT_FAILURE);
			break;
		
	}

	return personajePorNivel->ultimoMovimiento;

}

void recibirMensajeTurno(personajeIndividual_t *personajePorNivel){
	tMensaje tipoMensaje;
	char* sPayload;

	char *msjInfo = malloc(100); //Lo agrego para saber que hilo de cual nivel hizo la solicitud
	sprintf(msjInfo, "%s: Espero Turno", personajePorNivel->nomNivel);
	int response = recibirPaquete(personajePorNivel->socketPlataforma, &tipoMensaje, &sPayload, logger, msjInfo);

	if (response <= 0) {
		log_info(logger, "Se murio la plataforma, CHAUUUUUUUUUUUUUUUUUU!");
		exit(EXIT_FAILURE);
	}

	free(msjInfo);

	switch (tipoMensaje) {
		case PL_OTORGA_TURNO:
			log_info(logger, "%s: Se recibe turno", personajePorNivel->nomNivel);
			break;

		case PL_MUERTO_POR_ENEMIGO:
			log_info(logger, "%s: El personaje se murio por enemigos mientra que recibia turno", personajePorNivel->nomNivel);
			personajePorNivel->murioEnNivel=true;
			break;

		case PL_MUERTO_POR_DEADLOCK:
			log_info(logger, "%s: El personaje se murio por deadlock", personajePorNivel->nomNivel);
			personajePorNivel->murioEnNivel=true;
			break;

		default:
			log_error(logger, "%s: Llego un mensaje (tipoMensaje: %s) cuando debia llegar PL_OTORGA_TURNO", personajePorNivel->nomNivel, enumToString(tipoMensaje));
			exit(EXIT_FAILURE);
			break;
	}
}

void pedirPosicionRecurso(personajeIndividual_t* personajePorNivel, char* recurso){

	tMensaje tipoMensaje;
	tPregPosicion solicitudRecurso;
	solicitudRecurso.simbolo=personaje.simbolo;
	solicitudRecurso.recurso= *recurso;

	tPaquete pkgSolicitudRecurso;
	serializarPregPosicion(P_POS_RECURSO, solicitudRecurso, &pkgSolicitudRecurso);
	char *msjInfo = malloc(100); //Lo agrego para saber que hilo de cual nivel hizo la solicitud
	sprintf(msjInfo, "%s: Solicito la posicion de un recurso", personajePorNivel->nomNivel);

	enviarPaquete(personajePorNivel->socketPlataforma, &pkgSolicitudRecurso, logger, msjInfo);

	char* sPayload;
	sprintf(msjInfo, "%s: Recibo posicion del recurso", personajePorNivel->nomNivel);
	int response = recibirPaquete(personajePorNivel->socketPlataforma, &tipoMensaje, &sPayload, logger, msjInfo);

	if (response <= 0) {
		log_info(logger, "Se murio la plataforma, CHAUUUUUUUUUUUUUUUUUU!");
		exit(EXIT_FAILURE);
	}

	free(msjInfo);

	switch (tipoMensaje){
		case PL_POS_RECURSO:{
			tRtaPosicion* rtaSolicitudRecurso;
			rtaSolicitudRecurso = deserializarRtaPosicion(sPayload);
			personajePorNivel->posRecursoX = rtaSolicitudRecurso->posX;
			personajePorNivel->posRecursoY = rtaSolicitudRecurso->posY;
			log_debug(logger, "%s: Recurso %c en posicion (%d, %d)", personajePorNivel->nomNivel, *recurso, personajePorNivel->posRecursoX, personajePorNivel->posRecursoY);
			break;
		}
		case PL_RECURSO_INEXISTENTE:{
			log_error(logger, "%s: El recurso %c no existe", personajePorNivel->nomNivel, recurso);
			exit(EXIT_FAILURE);
			break;
		}
		case PL_MUERTO_POR_ENEMIGO:{
			log_info(logger, "%s: El personaje se murio por enemigos mientra que recibia turno", personajePorNivel->nomNivel);
			personajePorNivel->murioEnNivel=true;
			break;
		}
		default:{
			log_error(logger, "%s: Llego un mensaje (tipoMensaje: %s) cuando debia llegar PL_POS_RECURSO", personajePorNivel->nomNivel, enumToString(tipoMensaje));
			exit(EXIT_FAILURE);
			break;
		}
	}
}

void handshake_plataforma(personajeIndividual_t* personajePorNivel){
	tMensaje tipoMensaje;
	tHandshakePers handshakePers;
	handshakePers.simbolo = personaje.simbolo;
	handshakePers.nombreNivel = malloc(sizeof(personajePorNivel->nomNivel));
	strcpy(handshakePers.nombreNivel, personajePorNivel->nomNivel);
	/* Se crea el paquete */
	tPaquete pkgHandshake;
	serializarHandshakePers(P_HANDSHAKE, handshakePers, &pkgHandshake);
	char *msjInfo = malloc(100); //Lo agrego para saber que hilo de cual nivel hizo la solicitud
	sprintf(msjInfo, "%s: Se envia saludo a la plataforma", personajePorNivel->nomNivel);

	enviarPaquete(personajePorNivel->socketPlataforma, &pkgHandshake, logger, msjInfo);

	char* sPayload;
	sprintf(msjInfo, "%s: Recibo si existe el nivel solicitado", personajePorNivel->nomNivel);
	int response = recibirPaquete(personajePorNivel->socketPlataforma, &tipoMensaje, &sPayload, logger, msjInfo);

	if (response <= 0) {
		log_info(logger, "Se murio la plataforma, CHAUUUUUUUUUUUUUUUUUU!");
		exit(EXIT_FAILURE);
	}

	free(msjInfo);

	switch(tipoMensaje){
		case PL_HANDSHAKE:
			log_info(logger, "%s: La plataforma le devolvio el handshake al personaje correctamente", personajePorNivel->nomNivel);
			break;

		case PL_NIVEL_INEXISTENTE:
			log_info(logger, "%s: El nivel requerido por el personaje no existe.", personajePorNivel->nomNivel);
			exit(EXIT_FAILURE);
			break;

		case PL_PERSONAJE_REPETIDO:
			log_error(logger, "%s: Se esta tratando de conectar un personaje que ya esta conectado con la plataforma", personajePorNivel->nomNivel);
			exit(EXIT_FAILURE);
			break;

		default:
			log_error(logger, "%s: Llego un mensaje (tipoMensaje: %s) cuando debia llegar PL_HANDSHAKE", personajePorNivel->nomNivel, enumToString(tipoMensaje));
			exit(EXIT_FAILURE);
			break;

	}
}

void cerrarConexion(int * socketPlataforma){
	close(*socketPlataforma);
	log_debug(logger, "La conexion con la plataforma ha sido cerrada.");
}


void calculaMovimiento(personajeIndividual_t *personajePorNivel){

	if(!conseguiRecurso(personajePorNivel)){
		switch (personajePorNivel->ultimoMovimiento)
		{
			case derecha: case izquierda:
				//ver si se puede mover para abajo o arriba
				if(tieneMovimientoVertical(*personajePorNivel))
					moverVertical(personajePorNivel);
				else if(tieneMovimientoHorizontal(*personajePorNivel))
					moverHorizontal(personajePorNivel);

				break;

			case arriba: case abajo:
				if(tieneMovimientoHorizontal(*personajePorNivel))
					moverHorizontal(personajePorNivel);
				else if(tieneMovimientoVertical(*personajePorNivel))
					moverVertical(personajePorNivel);

				break;

			case vacio:
				if(tieneMovimientoVertical(*personajePorNivel))
					moverVertical(personajePorNivel);
				else if(tieneMovimientoHorizontal(*personajePorNivel))
					moverHorizontal(personajePorNivel);

				if(tieneMovimientoHorizontal(*personajePorNivel))
					moverHorizontal(personajePorNivel);
				else if(tieneMovimientoVertical(*personajePorNivel))
					moverVertical(personajePorNivel);

				break;

			default:
				log_error(logger, "El ultimo movimiento realizado no fue un movimiento permitido.");
				exit(EXIT_FAILURE);
				break;

		}
	}
}

bool tieneMovimientoVertical(personajeIndividual_t personajePorNivel){
	return (personajePorNivel.posY != personajePorNivel.posRecursoY);
}

bool tieneMovimientoHorizontal(personajeIndividual_t personajePorNivel){
	return (personajePorNivel.posX != personajePorNivel.posRecursoX);
}

void moverHorizontal(personajeIndividual_t *personajePorNivel){

	if (personajePorNivel->posX < personajePorNivel->posRecursoX)
		(*personajePorNivel).ultimoMovimiento = derecha;
	else if (personajePorNivel->posX > personajePorNivel->posRecursoX)
		(*personajePorNivel).ultimoMovimiento = izquierda;

}

void moverVertical(personajeIndividual_t *personajePorNivel){

	if (personajePorNivel->posY < personajePorNivel->posRecursoY)
		personajePorNivel->ultimoMovimiento = abajo;
	else if (personajePorNivel->posY > personajePorNivel->posRecursoY)
		personajePorNivel->ultimoMovimiento = arriba;

}

void actualizaPosicion(tDirMovimiento* movimiento, personajeIndividual_t *personajePorNivel) {
	// Actualiza las variables posicion del personaje a partir del movimiento que recibe por parametro.
	// El eje Y es alreves, por eso para ir para arriba hay que restar en el eje y.
	switch (*movimiento) {
		case arriba:
			personajePorNivel->posY--;
			break;
		case abajo:
			personajePorNivel->posY++;
			break;
		case derecha:
			personajePorNivel->posX++;
			break;
		case izquierda:
			personajePorNivel->posX--;
			break;
		default:
			break;
	}
	
}

void sig_aumentar_vidas() {
	pthread_mutex_lock(&semModificadorDeVidas);
	personaje.vidas++;
	pthread_mutex_unlock(&semModificadorDeVidas);
	log_debug(logger, "Se le ha agregado una vida por senial.");
	log_debug(logger, "Vidas de %c = %d", personaje.simbolo, personaje.vidas);
}

void restarVida(){

	pthread_mutex_lock(&semModificadorDeVidas);
	personaje.vidas--; //TODO Validar si no le quedaran vidas

	if (personaje.vidas <= 0){
		log_debug(logger, "El personaje %c detecto que no hay mas vidas\n", personaje.simbolo);
		matarHilos();
		desconectarPersonajeDeTodoNivel();
		reiniciar = consultarReinicio(); //Aqui es donde me asesino
		if (reiniciar) {
			sem_post(&semReinicio); //SIGNAL(sem)
			sem_post(&semFinPlan);
		}
	}
	pthread_mutex_unlock(&semModificadorDeVidas);
	log_debug(logger, "Se le ha restado una vida.");
	log_debug(logger, "Vidas de %c = %d", personaje.simbolo, personaje.vidas);
}

//Para la señal SIGINT
void matarHilos() {
	int cantidadPersonajes = list_size(personaje.lPersonajes);
	int indexPersonaje;
	personajeIndividual_t *pPersonajePorNivel;
	for (indexPersonaje = 0; indexPersonaje < cantidadPersonajes; indexPersonaje++) {
		pPersonajePorNivel = list_get(personaje.lPersonajes, indexPersonaje);
		pthread_cancel(*pPersonajePorNivel->thread);
	}
}

//Para la Señal SIGINT
void desconectarPersonajeDeTodoNivel() {
	//desconecta a todos los personajes por nivel

	int cantidadPersonajes = list_size(personaje.lPersonajes);
	int indexPersonaje;
	personajeIndividual_t *pPersonajePorNivel;

	for (indexPersonaje = 0; indexPersonaje < cantidadPersonajes; indexPersonaje++) {
		pPersonajePorNivel = list_get(personaje.lPersonajes, indexPersonaje);
		desconectarPersonaje(pPersonajePorNivel);
	}

}

void muertoPorSenial() {

	pthread_mutex_lock(&semModificadorDeVidas);
	personaje.vidas=0;

	log_info(logger, "Capture la señal SIGINT y ahora debo morir");

	matarHilos();
	desconectarPersonajeDeTodoNivel();

	pthread_mutex_unlock(&semModificadorDeVidas);

	log_info(logger, "El personaje ha muerto por la senal kill");

	pthread_mutex_destroy(&semModificadorDeVidas);

	log_destroy(logger);
	config_destroy(configPersonaje);

	int cantidadPersonajes = list_size(personaje.lPersonajes);
	int indexPersonaje;
	personajeIndividual_t *pPersonajePorNivel;

	for (indexPersonaje = 0; indexPersonaje < cantidadPersonajes; indexPersonaje++) {
		pPersonajePorNivel = list_get(personaje.lPersonajes, indexPersonaje);
		list_destroy(pPersonajePorNivel->Objetivos);
	}

	list_destroy(personaje.lPersonajes);

	exit(EXIT_FAILURE);
}

bool consultarReinicio(){
	char cRespuesta[50];

	printf("El personaje ha muerto ¿Desea volver a intentar? (Y/N) ");

	scanf("%s", cRespuesta);

	char caracter_pulsado = *cRespuesta;
	while ((toupper(caracter_pulsado) != 'N') && (toupper(caracter_pulsado) != 'Y')) {
		printf("No entiendo ese comando \n");
		printf("¿Desea volver a intentar? (Y/N) \n");
		scanf("%s", cRespuesta);
	}

	if (toupper(caracter_pulsado) == 'Y') {
		personaje.reintentos++;
		personaje.vidas = personaje.vidasMaximas;
		log_info(logger, "Se vuelve a jugar. Numero de reintentos: %d");
		return true;

	} else if (toupper(caracter_pulsado) == 'N') {
		return false;
	}
	return false;

}

