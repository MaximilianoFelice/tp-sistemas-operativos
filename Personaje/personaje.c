/*
 * Sistemas Operativos - Super Mario Proc RELOADED.
 * Grupo       : C o no ser.
 * Nombre      : personaje.c.
 * Descripcion : Este archivo contiene la implementacion de las
 * funciones usadas por el personaje.
 */

#include "personaje.h"

pthread_mutex_t mtxlPersPorNivel;
pthread_mutex_t semMovement;
pthread_mutex_t semModificadorDeVidas;
bool continuar = false;

personajeGlobal_t personaje;
t_config *configPersonaje;
t_log *logger;
char * ip_plataforma;
char * puerto_orq;

int socketOrquestador;
int r = 0;
//bool muertePorSenial=false;

threadNivel_t *hilosNiv;
t_dictionary *listaPersonajePorNiveles; //diccionario
int cantidadNiveles;


int main(int argc, char*argv[]) {

	pthread_mutex_init(&semMovement, NULL);
	pthread_mutex_init(&semModificadorDeVidas, NULL);
	pthread_mutex_init(&mtxlPersPorNivel, NULL);

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
	cantidadNiveles =list_size(personaje.listaNiveles);
	hilosNiv = calloc(cantidadNiveles, sizeof(threadNivel_t));
	personaje.vidas = personaje.vidasMaximas;


	//Me conecto con la plataforma para despues de terminar todos los niveles correctamente avisarle
	socketOrquestador= connectToServer(ip_plataforma, atoi(puerto_orq), logger);
	log_debug(logger, "El personaje se conecto con el orquestador");

	int i;
	for( i = 0;  i < cantidadNiveles; i ++) {
		nivel_t *nivelLevantador = (nivel_t *)list_get(personaje.listaNiveles, i);
		//creo estructura del nivel que va a jugar cada hilo
		hilosNiv[i].nivel.nomNivel = nivelLevantador->nomNivel;
		hilosNiv[i].nivel.Objetivos = nivelLevantador->Objetivos;
		hilosNiv[i].nivel.num_of_thread = i;

		//Tiro el hilo para jugar de cada nivel
		if (pthread_create(&hilosNiv[i].thread, NULL, jugar, (void *) &hilosNiv[i].nivel)) {
			log_error(logger, "pthread_create: %s", strerror(errno));
			exit(EXIT_FAILURE);
		}
	}

	char *join_return;
	continuar = true;
	while(continuar){
		continuar = false;
		for (i = 0; i < cantidadNiveles; i++) {
			pthread_join(hilosNiv[i].thread, (void**)&join_return);
		}
	}

	if (personaje.vidas>0)//Termino t0do el plan de niveles correctamente
	{
		notificarFinPlanNiveles();//Le avisa al orquestador que se termino correctamente el plan de todos los niveles

	}

	cerrarConexiones(&socketOrquestador);
	log_debug(logger, "El personaje se desconecto del orquestador");

	if(join_return != NULL)
		log_debug(logger, "El personaje %c %s", personaje.simbolo, join_return);

	log_destroy(logger);
	destruirArchivoConfiguracion(configPersonaje);
	for (i = 0; i < list_size(personaje.listaNiveles); i++) {
		nivel_t *aux = (nivel_t *) list_get(personaje.listaNiveles, i);
		list_destroy(aux->Objetivos);
	}
	list_destroy_and_destroy_elements(personaje.listaNiveles, (void *) nivel_destroyer);

	pthread_mutex_destroy(&semMovement);

	exit(EXIT_SUCCESS);

}

void notificarFinPlanNiveles(){
	tPaquete pkgDevolverRecursos;
	pkgDevolverRecursos.type   = P_FIN_PLAN_NIVELES;
	pkgDevolverRecursos.length = 0;

	enviarPaquete(socketOrquestador, &pkgDevolverRecursos, logger, "Se notifica al orquestador la finalizacion del plan de niveles del personaje correctamente");
}

void destruirArchivoConfiguracion(t_config *configPersonaje){

	config_destroy(configPersonaje);

}

void cargarArchivoConfiguracion(char* archivoConfiguracion){

	//valida que los campos basicos esten en el archivo
	configPersonaje = config_try_create(archivoConfiguracion, "nombre,simbolo,planDeNiveles,vidas,orquestador");

	// Obtenemos el nombre del personaje - global de solo lectura
	personaje.nombre = config_get_string_value(configPersonaje, "nombre");

	personaje.reintentos = 0;

	// Obtenemos el simbolo - global de solo lectura
	personaje.simbolo = config_get_string_value(configPersonaje, "simbolo")[0];

	personaje.vidasMaximas = config_get_int_value(configPersonaje, "vidas"); //Obtenemos las vidas

	// Obetenemos los datos del orquestador
	char * dir_orq = config_get_string_value(configPersonaje, "orquestador");
	obtenerIpYPuerto(dir_orq, ip_plataforma, puerto_orq);

	//Obtenemos el plan de niveles
	char** niveles = config_try_get_array_value(configPersonaje, "planDeNiveles");
	t_list* listaObjetivos;
	personaje.listaNiveles = list_create();
	int j, i = 0;
	nivel_t aux;

	listaPersonajePorNiveles = dictionary_create();

	char *stringABuscar = malloc(sizeof(char) * 25);


	//Armamos lista de niveles con sus listas de objetivos del config
	while (niveles[i] != NULL ) {  //Cicla los niveles

		sprintf(stringABuscar, "obj[%s]", niveles[i]); //Arma el string a buscar

		char** objetivos = config_try_get_array_value(configPersonaje, stringABuscar);

		j = 0;

		//Por cada nivel, genero una lista de objetivos
		listaObjetivos = list_create();
		while (objetivos[j] != NULL ) { //Por cada objetivo
			list_add_new(listaObjetivos, objetivos[j], sizeof(char)); //Agrego a la lista
			j++;
		}

		//agregamos el nivel a la lista de niveles
		aux.nomNivel = malloc(sizeof(char) * strlen(niveles[i]));
		strcpy(aux.nomNivel, niveles[i]);
		aux.Objetivos = listaObjetivos;
		list_add_new(personaje.listaNiveles, &aux, sizeof(nivel_t));

		i++;
	}
	free(stringABuscar);

}

void obtenerIpYPuerto(char *dirADividir, char * ip,  char * puerto){
	ip_plataforma  = strtok(dirADividir, ":"); // Separar ip
	puerto_orq 	   = strtok(NULL, ":"); // Separar puerto

}

static void nivel_destroyer(nivel_t*nivel) {
	free(nivel->nomNivel);
	free(nivel);
}

void *jugar(void *args) {

	//Recupero la informacion del nivel en el que juega el personaje

	personajeIndividual_t personajePorNivel;

	personajePorNivel.posX=0;
	personajePorNivel.posY=0;
	personajePorNivel.nivelQueJuego = (nivel_t *) args;
	personajePorNivel.ultimoMovimiento=vacio;

	bool murioPersonaje = false;

	personajePorNivel.socketPlataforma = connectToServer(ip_plataforma, atoi(puerto_orq), logger);

	pthread_mutex_lock(&mtxlPersPorNivel);
	dictionary_put(listaPersonajePorNiveles, personajePorNivel.nivelQueJuego->nomNivel, &personajePorNivel);
	pthread_mutex_unlock(&mtxlPersPorNivel);

	handshake_plataforma(&personajePorNivel);

	bool finalizoPlanNivel = false;

	while ((personaje.vidas  > 0) & (!finalizoPlanNivel)) {

		log_info(logger, "Vidas de %c: %d", personaje.simbolo, personaje.vidas);

		// Por cada objetivo del nivel, fixme, revisar si hace falta q controle si esta o no muerto
		for (personajePorNivel.recursoActual=0; (personajePorNivel.recursoActual < list_size(personajePorNivel.nivelQueJuego->Objetivos)) && (!personajeEstaMuerto(murioPersonaje)); personajePorNivel.recursoActual++) {

			//Espera que el planificador le de el turno para pedir la posicion del recurso
			recibirMensajeTurno(&personajePorNivel);

			//agarra un recurso de la lista de objetivos del nivel
			char* recurso = (char*) list_get(personajePorNivel.nivelQueJuego->Objetivos, personajePorNivel.recursoActual);
			pedirPosicionRecurso(&personajePorNivel, recurso);
			log_debug(logger, "Ya tengo la posicion de mi recurso");

			while (!conseguiRecurso(personajePorNivel) && !personajeEstaMuerto(murioPersonaje)) {

				//Espera que el planificador le de el turno para moverse
				recibirMensajeTurno(&personajePorNivel);

				//El personaje se mueve
				moverAlPersonaje(&personajePorNivel);

			}

			//Espera que el planificador le de el turno para solicitar el recurso
			recibirMensajeTurno(&personajePorNivel);

			if (!personajeEstaMuerto(murioPersonaje))
				solicitarRecurso(&personajePorNivel, recurso);

		} //Fin de for de objetivos

		finalizoPlanNivel = true;

	}

	if (!murioPersonaje){
		log_info(logger, "El personaje ha completado el nivel correctamente.");
	}

	manejarDesconexiones(&personajePorNivel, murioPersonaje);

	desconectarPersonaje(&personajePorNivel);
	log_debug(logger, "El personaje se desconecto de la plataforma");

	char * exit_return = strdup("El personaje ha finalizado su plan de nivel");
	pthread_exit((void *)exit_return);
}

void desconectarPersonaje(personajeIndividual_t* personajePorNivel){
	cerrarConexiones(&personajePorNivel->socketPlataforma);
	personajePorNivel->socketPlataforma=0; // NOTA: esto es para la desconexion de todos los personajes cuando se reinicia el juego
}

void manejarDesconexiones(personajeIndividual_t* personajePorNivel, bool murioPersonaje){//, bool* finalice){
	if (!murioPersonaje)
		log_info(logger, "El personaje ha completado el nivel.");

	desconectarPersonaje(personajePorNivel);
	log_debug(logger, "El personaje se desconecto de la plataforma");
}

bool personajeEstaMuerto(bool murioPersonaje){
	//si esta muerto por alguna señal o porque se quedo sin vidas
	return (murioPersonaje || personaje.vidas<=0);
}

bool conseguiRecurso(personajeIndividual_t personajePorNivel){
	return ((personajePorNivel.posY == personajePorNivel.posRecursoY) && (personajePorNivel.posX == personajePorNivel.posRecursoX));
}

void moverAlPersonaje(personajeIndividual_t* personajePorNivel){
	tDirMovimiento  mov;

	mov = calcularYEnviarMovimiento(personajePorNivel);
	//Actualizo mi posición y de acuerdo a eso armo mensaje de TURNO

	pthread_mutex_lock(&semMovement);
	actualizaPosicion(&mov, &personajePorNivel);
	pthread_mutex_unlock(&semMovement);

}

void solicitarRecurso(personajeIndividual_t* personajePorNivel, char *recurso){

	tMensaje tipoMensaje;
	tPregPosicion recursoSolicitado;
	recursoSolicitado.simbolo=personaje.simbolo;
	recursoSolicitado.recurso=*recurso;

	tPaquete pkgSolicitudRecurso;
	serializarPregPosicion(P_SOLICITUD_RECURSO, recursoSolicitado, &pkgSolicitudRecurso);

	log_debug(logger, "Se envia solicitud del recurso a la plataforma");
	enviarPaquete(personajePorNivel->socketPlataforma, &pkgSolicitudRecurso, logger, "El personaje le envia la solicitud del recurso a la plataforma");

	char* sPayload;
	recibirPaquete(personajePorNivel->socketPlataforma, &tipoMensaje, &sPayload, logger, "El personaje recibe respuesta de la solicitud del recurso a la plataforma");

	switch(tipoMensaje){
		case PL_RECURSO_OTORGADO:{
			log_info(logger, "El personaje recibe el recurso");
			break;
		}
		case PL_RECURSO_INEXISTENTE:{
			log_error(logger, "El recurso pedido por el personaje no existe.");
			exit(EXIT_FAILURE);
			break;
		}
		case PL_MUERTO_POR_ENEMIGO:{
			log_info(logger, "El personaje se murio por enemigos");
			seMuereSinSenal(personajePorNivel);
			break;
		}
		case PL_MUERTO_POR_DEADLOCK:{
			log_info(logger, "El personaje se murio por deadlock");
			seMuereSinSenal(personajePorNivel);
			break;
		}
		default: {
			log_error(logger, "Llego un mensaje (tipoMensaje: %s) cuando debia llegar PL_SOLICITUD_RECURSO", enumToString(tipoMensaje));
			log_error(logger, "Estoy en un sleep(400000000); porque me llego mal un mensaje: DETENGO TODO");
			sleep(40000000);
			exit(EXIT_FAILURE);
			break;
		}
	}

}

void seMuereSinSenal(personajeIndividual_t *personajePorNivel){

	desconectarPersonaje(personajePorNivel);
	if (personaje.vidas>0){
		restarVida();
		/*
		todo
 	 	 vuelve a conectarse con la plataforma(planificador)
 	 	 reinicia SOLO ese nivel, como?:
 	 	 mata el hilo
 	 	 reinicia los objetivos de ese nivel
 	 	 vuelve a tirar ese hilo

 	 	 		 * */
	}else{
		reiniciarJuego();
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

	log_debug(logger, "Se envia paquete con el pedido de movimiento");
	enviarPaquete(personajePorNivel->socketPlataforma, &pkgMovimiento, logger, "Envio pedido de movimiento del personaje");

	char* sPayload;
	recibirPaquete(personajePorNivel->socketPlataforma, &tipoMensaje, &sPayload, logger, "Se espera confirmacion del movimiento");

	switch(tipoMensaje){
		case PL_MUERTO_POR_ENEMIGO:{
			log_info(logger, "El personaje se murio por enemigos");
			seMuereSinSenal(personajePorNivel);
			break;
		}
		case PL_MUERTO_POR_DEADLOCK:{
			log_info(logger, "El personaje se murio por deadlock");
			seMuereSinSenal(personajePorNivel);
			break;
		}
		case PL_CONFIRMACION_MOV:{
			log_info(logger, "Movimiento confirmado");
			break;
		}
		default: {
			log_error(logger, "Llego un mensaje (tipoMensaje: %s) cuando debia llegar PL_CONFIRMACION_MOV", enumToString(tipoMensaje));
			log_error(logger, "Estoy en un sleep(400000000); porque me llego mal un mensaje: DETENGO TODO");
			sleep(40000000);
			exit(EXIT_FAILURE);
			break;
		}
	}

	return personajePorNivel->ultimoMovimiento;

}

void recibirMensajeTurno(personajeIndividual_t *personajePorNivel){
	tMensaje tipoMensaje;
	char* sPayload;

	do {
		recibirPaquete(personajePorNivel->socketPlataforma, &tipoMensaje, &sPayload, logger, "Espero turno");

		switch (tipoMensaje) {
			case PL_OTORGA_TURNO:
				log_info(logger, "Se recibe turno");
				break;

			case PL_MUERTO_POR_ENEMIGO:
				log_info(logger, "El personaje se murio por enemigos");
				seMuereSinSenal(personajePorNivel);
				break;

			case PL_MUERTO_POR_DEADLOCK:
				log_info(logger, "El personaje se murio por deadlock");
				seMuereSinSenal(personajePorNivel);
				break;

			default:
				log_error(logger, "Llego un mensaje (tipoMensaje: %s) cuando debia llegar PL_OTORGA_TURNO", enumToString(tipoMensaje));
				break;
		}

	} while (tipoMensaje != PL_OTORGA_TURNO);
}

void pedirPosicionRecurso(personajeIndividual_t* personajePorNivel, char* recurso){

	tMensaje tipoMensaje;
	tPregPosicion solicitudRecurso;
	solicitudRecurso.simbolo=personaje.simbolo;
	solicitudRecurso.recurso= *recurso;

	tPaquete pkgSolicitudRecurso;
	serializarPregPosicion(P_POS_RECURSO, solicitudRecurso, &pkgSolicitudRecurso);

	enviarPaquete(personajePorNivel->socketPlataforma, &pkgSolicitudRecurso, logger, "Solicito la posicion de un recurso");

	char* sPayload;
	recibirPaquete(personajePorNivel->socketPlataforma, &tipoMensaje, &sPayload, logger, "Recibo posicion del recurso");

	switch (tipoMensaje){
		case PL_POS_RECURSO:{
			tRtaPosicion* rtaSolicitudRecurso;
			rtaSolicitudRecurso = deserializarRtaPosicion(sPayload);

			personajePorNivel->posRecursoX = rtaSolicitudRecurso->posX;
			personajePorNivel->posRecursoY = rtaSolicitudRecurso->posY;

			break;
		}
		case PL_RECURSO_INEXISTENTE:{
			log_error(logger, "El recurso %c no existe, reintentar pedido", recurso);
			reintentarSolicitudRecurso(personajePorNivel,&pkgSolicitudRecurso, recurso);
			break;
		}
		default:{
			log_error(logger, "Llego un mensaje (tipoMensaje: %s) cuando debia llegar PL_POS_RECURSO", enumToString(tipoMensaje));
			log_error(logger, "Estoy en un sleep(400000000); porque me llego mal un mensaje: DETENGO TODO");
			sleep(40000000);
			exit(EXIT_FAILURE);
			break;
		}
	}
}

void reintentarSolicitudRecurso(personajeIndividual_t* personajePorNivel, tPaquete* pkgHandshake, char* recurso){

	sleep(800); //espero un poquito antes de conectarme de nuevo

	enviarPaquete(personajePorNivel->socketPlataforma, pkgHandshake, logger, "Se reenvia solicitud de la posicion del recurso a la plataforma");

	tMensaje tipoMensaje;
	char* sPayload;
	recibirPaquete(personajePorNivel->socketPlataforma, &tipoMensaje, &sPayload, logger, "Recibo estado en el que quedo el personaje");

	switch (tipoMensaje){
		case PL_POS_RECURSO:{
			tRtaPosicion* rtaSolicitudRecurso;
			rtaSolicitudRecurso = deserializarRtaPosicion(sPayload);

			personajePorNivel->posRecursoX = rtaSolicitudRecurso->posX;
			personajePorNivel->posRecursoY = rtaSolicitudRecurso->posY;

			break;
		}
		case PL_RECURSO_INEXISTENTE:{
			log_error(logger, "El recurso %c no existe, reintentar pedido", *recurso);
			break;
		}
		default:{
			log_error(logger, "Llego un mensaje (tipoMensaje: %d) cuando debia llegar PL_POS_RECURSO", tipoMensaje);
			log_error(logger, "Estoy en un sleep(400000000); porque me llego mal un mensaje: DETENGO TODO");
			sleep(40000000);
			exit(EXIT_FAILURE);
			break;
		}
	}
}

void handshake_plataforma(personajeIndividual_t* personajePorNivel){
	tMensaje tipoMensaje;
	tHandshakePers handshakePers;
	handshakePers.simbolo = personaje.simbolo;
	handshakePers.nombreNivel = malloc(sizeof(personajePorNivel->nivelQueJuego->nomNivel));
	strcpy(handshakePers.nombreNivel, personajePorNivel->nivelQueJuego->nomNivel);
	/* Se crea el paquete */
	tPaquete pkgHandshake;
	serializarHandshakePers(P_HANDSHAKE, handshakePers, &pkgHandshake);

	enviarPaquete(personajePorNivel->socketPlataforma, &pkgHandshake, logger, "Se envia saludo a la plataforma");

	char* sPayload;
	recibirPaquete(personajePorNivel->socketPlataforma, &tipoMensaje, &sPayload, logger, "Recibo si existe el nivel solicitado");

	switch(tipoMensaje){
		case PL_HANDSHAKE:{
			log_info(logger, "La plataforma le devolvio el handshake al personaje correctamente");
			break;
		}
		case PL_NIVEL_INEXISTENTE:{
			log_info(logger, "El nivel requerido por el personaje no existe.");
			reintentarHandshake(personajePorNivel->socketPlataforma, &pkgHandshake);
			break;
		}
		case PL_PERSONAJE_REPETIDO:{
			log_error(logger, "Se esta tratando de conectar un personaje que ya esta conectado con la plataforma");
			break;
		}
		default:
			break;

	}
}

void reintentarHandshake(int socketPlataforma, tPaquete* pkgHandshake){

	cerrarConexiones(&socketPlataforma);

	sleep(800); //espero un poquito antes de conectarme de nuevo

	socketPlataforma= connectToServer(ip_plataforma, atoi(puerto_orq), logger);

	enviarPaquete(socketPlataforma, pkgHandshake, logger, "Se envia saludo a la plataforma");

	tMensaje tipoMensaje;
	char* sPayload;
	recibirPaquete(socketPlataforma, &tipoMensaje, &sPayload, logger, "Recibo estado en el que quedo el personaje");

	//Recibo un aviso de que existe o no el nivel
	if (tipoMensaje == PL_NIVEL_INEXISTENTE){
		reintentarHandshake(socketPlataforma, pkgHandshake);
	}
}

void cerrarConexiones(int * socketPlataforma){
	close(*socketPlataforma);
	log_debug(logger, "La conexion con la plataforma ha sido cerrada.");
}


void calculaMovimiento(personajeIndividual_t *personajePorNivel){

	if(!conseguiRecurso(*personajePorNivel)){
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

void actualizaPosicion(tDirMovimiento* movimiento, personajeIndividual_t **personajePorNivel) {
	// Actualiza las variables posicion del personaje a partir del movimiento que recibe por parametro.
	// El eje Y es alreves, por eso para ir para arriba hay que restar en el eje y.
	switch (*movimiento) {
		case arriba:
			((*personajePorNivel)->posY)--;
			break;
		case abajo:
			((*personajePorNivel)->posY)++;
			break;
		case derecha:
			((*personajePorNivel)->posX)++;
			break;
		case izquierda:
			((*personajePorNivel)->posX)--;
			break;
	}
	//log_debug(logger, "P = (%d, %d) - R = (%d, %d)", (*personajePorNivel)->posX, (*personajePorNivel)->posY, (*personajePorNivel)->posRecursoX, (*personajePorNivel)->posRecursoY);

}

void sig_aumentar_vidas() {
	pthread_mutex_lock(&semModificadorDeVidas);
	personaje.vidas++;
	pthread_mutex_unlock(&semModificadorDeVidas);
	log_debug(logger, "Se le ha agregado una vida por senal.");
}

void restarVida(){

	pthread_mutex_lock(&semModificadorDeVidas);
	personaje.vidas--;

	if (personaje.vidas <= 0) {
		desconectarPersonajeDeTodoNivel();
		matarHilos();
		reiniciarJuego();
	}

	pthread_mutex_unlock(&semModificadorDeVidas);
	log_debug(logger, "Se le ha restado una vida.");
}

void desconectarPersonajeDeTodoNivel(){
	//desconecta a todos los personajes por nivel

	void _desconectarPersonaje(char* key, personajeIndividual_t* personajePorNivel) {
		desconectarPersonaje(personajePorNivel);
	}

	dictionary_iterator(listaPersonajePorNiveles, (void*) _desconectarPersonaje);
}

void muertoPorSenial(){

	pthread_mutex_lock(&semModificadorDeVidas);
	personaje.vidas=0;

	log_info(logger, "El personaje ha muerto por la senal kill");

	matarHilos();
	desconectarPersonajeDeTodoNivel();

	continuar=false;
	pthread_mutex_unlock(&semModificadorDeVidas);


	pthread_mutex_destroy(&semModificadorDeVidas);

	log_destroy(logger);
	destruirArchivoConfiguracion(configPersonaje);
	int i;
	for (i = 0; i < list_size(personaje.listaNiveles); i++) {
		nivel_t *aux = (nivel_t *) list_get(personaje.listaNiveles, i);
		list_destroy(aux->Objetivos);
	}
	list_destroy_and_destroy_elements(personaje.listaNiveles, (void *) nivel_destroyer);

	pthread_mutex_destroy(&semMovement);

	exit(EXIT_FAILURE);
}

void matarHilos(){
	/* matar a todos los threads */

	int i;
	for (i = 0; i < cantidadNiveles; i++) {
		pthread_cancel(hilosNiv->thread);
	}

}

void reiniciarJuego(){
	char n;

	log_info(logger, "El personaje murio por quedarse sin vidas.");
	log_debug(logger, "Se procede a desconectarse de la plataforma");

	matarHilos();
	desconectarPersonajeDeTodoNivel();
	log_debug(logger, "Ya se desconecto de la plataforma");

	printf("\n ¿Ya tiene %d reintentos, Desea volver a intentar? (Y/N) ", personaje.reintentos);

	n = getchar();

	while( (n != 'N') | (n != 'Y') ){
		n = getchar();
		printf("No entiendo ese comando");
		printf("\n ¿Desea volver a intentar? (Y/N) ");
	}

	if (n == 'Y'){
		personaje.reintentos++;

		/*todo
		 *reinicia los objetivos
		 *vuelve a tirar todos los hilos
		 *
		 */
		continuar =true;
		personaje.vidas = personaje.vidasMaximas;

	}

	if (n == 'N')
		continuar = false; //cierra el personaje


}
