/*
 * Sistemas Operativos - Super Mario Proc RELOADED.
 * Grupo       : C o no ser.
 * Nombre      : personaje.c.
 * Descripcion : Este archivo contiene la implementacion de las
 * funciones usadas por el personaje.
 */

#include "personaje.h"

pthread_mutex_t semMovement;
pthread_mutex_t semModificadorDeVidas;
bool continuar = false;

personajeGlobal_t personaje;
t_config *configPersonaje;
t_log *logger;
char * ip_plataforma;
char * puerto_orq;

int r = 0;
bool muertePorSenial=false;
bool inicializeVidas = false;
threadNivel_t *hilosNiv;
t_dictionary *listaPersonajePorNiveles; //diccionario
int cantidadNiveles;


void sig_aumentar_vidas(){
	pthread_mutex_lock(&semModificadorDeVidas);
	personaje.vidas++;
	pthread_mutex_unlock(&semModificadorDeVidas);
}


int main(int argc, char*argv[]) {

	pthread_mutex_init(&semMovement, NULL);
	pthread_mutex_init(&semModificadorDeVidas, NULL);

	// Inicializa el log.
	logger = logInit(argv, "PERSONAJE");

	if (signal(SIGINT, morirSenial) == SIG_ERR) {
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
	personaje.vidas = personaje.vidasMaximas; //TODO les agrgue esto porque en el while(personaje.vidas>0) preguntaba por basura

	int i;
	for( i = 0;  i < cantidadNiveles; i ++) {
		//creo estructura del nivel que va a jugar cada hilo
		hilosNiv[i].nivel.nomNivel = ((nivel_t*) list_get_data(personaje.listaNiveles,i))->nomNivel;
		hilosNiv[i].nivel.Objetivos = ((nivel_t*) list_get_data(personaje.listaNiveles,	i))->Objetivos;
		hilosNiv[i].nivel.num_of_thread = i;

		//Tiro el hilo para jugar de cada nivel
		if (pthread_create(&hilosNiv[i].thread, NULL, jugar, (void *) &hilosNiv[i].nivel)) {
			log_error(logger, "pthread_create: %s", strerror(errno));
			exit(EXIT_FAILURE);
		}

		i++;
	}

	char *join_return;
	continuar = true;
	while(continuar){
		continuar = false;
		for (i = 0; i < cantidadNiveles; i++) {
			pthread_join(hilosNiv[i].thread, (void**)&join_return);
		}
	}

	//Cuando terminaron todos los niveles. Manda msj al orquestador de que ya termino todos sus niveles
	int socketOrquestador = connectToServer(ip_plataforma, atoi(puerto_orq), logger);
	notificarFinPlanNiveles(socketOrquestador);
	cerrarConexiones(&socketOrquestador);

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


void notificarFinPlanNiveles(int socketOrquestador){
	tPaquete pkgDevolverRecursos;
	pkgDevolverRecursos.type   = P_FIN_PLAN_NIVELES;
	pkgDevolverRecursos.length = 0;
	enviarPaquete(socketOrquestador, &pkgDevolverRecursos, logger, "Se notifica a la plataforma la finalizacion del plan de niveles del personaje");

}

void destruirArchivoConfiguracion(t_config *configPersonaje){
	config_destroy(configPersonaje);
}

void cargarArchivoConfiguracion(char* archivoConfiguracion){

	//valida que los campos basicos esten en el archivo
	configPersonaje = config_try_create(archivoConfiguracion, "nombre,simbolo,planDeNiveles,vidas,orquestador");

	// Obtenemos el nombre del personaje - global de solo lectura
	personaje.nombre = config_get_string_value(configPersonaje, "nombre");

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

	//bool terminoPlanNiveles = false;

	personajeIndividual_t personajePorNivel;

	personajePorNivel.posX=0;
	personajePorNivel.posY=0;
	personajePorNivel.nivelQueJuego = (nivel_t *) args;

	bool finalice = false;
	bool murioPersonaje = false;

	personajePorNivel.socketPlataforma= connectToServer(ip_plataforma, atoi(puerto_orq), logger);

	dictionary_put(listaPersonajePorNiveles, personajePorNivel.nivelQueJuego->nomNivel, &personajePorNivel);

	handshake_plataforma(&personajePorNivel);

	//Setea los flags del inicio
	finalice = false;
	murioPersonaje = false;

	while (personaje.vidas  > 0) {

		murioPersonaje = false;
		finalice = false;

		log_info(logger, "Vidas de %c: %d", personaje.simbolo, personaje.vidas);

		// Por cada objetivo del nivel,
		for (personajePorNivel.objetivoActual=0; (personajePorNivel.objetivoActual < list_size(personajePorNivel.nivelQueJuego->Objetivos)) && (!personajeEstaMuerto(murioPersonaje)); personajePorNivel.objetivoActual++) {

			murioPersonaje = false;

			//agarra un recurso de la lista de objetivos del nivel
			char* recurso = (char*) list_get_data(personajePorNivel.nivelQueJuego->Objetivos, personajePorNivel.objetivoActual);
			pedirPosicionRecurso(&personajePorNivel, recurso);

			while (!conseguiRecurso(personajePorNivel)) {

				/* El thread se va a mover, y no quiere que otro thread se mueva y pueda perder vidas */
				pthread_mutex_lock(&semMovement);

				//Espera que el planificador le de el turno
				recibirMensajeTurno(personajePorNivel.socketPlataforma);

				log_info(logger, "Habemus turno");

				//El personaje se mueve
				moverAlPersonaje(&personajePorNivel);

				pthread_mutex_unlock(&semMovement);

			}  // Termina el plan de objetivos o muere.

			/* todo  Si las vidas son mayores a 0, significa que termino su plan de objetivos */
			/*if (personaje.vidas > 0)
				terminoPlanNiveles = true;*/

		} //Fin de for de objetivos


		if(muertePorSenial || finalice)
			break;

	} //Fin del while(vidas>0)



	manejarDesconexiones(&personajePorNivel, murioPersonaje, &finalice);

	// -----------------
	if(murioPersonaje){
		restarVida();
		if (personaje.vidas<=0) {
			devolverRecursosPorMuerte(personajePorNivel.socketPlataforma);
		}
		log_info(logger, "Me han matado :/");
		cerrarConexiones(&personajePorNivel.socketPlataforma);
	}

	// --------

	if (personaje.vidas<=0) {
		char *exit_return;
		exit_return = strdup("se ha quedado sin vidas y murio :'(");
		pthread_exit((void *)exit_return);
	}
	if (muertePorSenial) {
		char *exit_return;
		exit_return = strdup("ha terminado por senial SIGINT");
		pthread_exit((void *)exit_return);
	}

	char * exit_return = strdup("ha finalizado su plan de niveles correctamente");
	pthread_exit((void *)exit_return);


}

void desconectarPersonaje(personajeIndividual_t* personajePorNivel){
	cerrarConexiones(&personajePorNivel->socketPlataforma);
	personajePorNivel->socketPlataforma=0;// NOTA: esto es para la desconexion de todos los personajes cuando se reinicia el juego
}

void manejarDesconexiones(personajeIndividual_t* personajePorNivel, bool murioPersonaje, bool* finalice){
	if (!murioPersonaje) {
		*finalice = true;
		devolverRecursosPorFinNivel(personajePorNivel->socketPlataforma);
		desconectarPersonaje(personajePorNivel);

	}

	if(personaje.vidas <= 0){
		devolverRecursosPorMuerte(personajePorNivel->socketPlataforma);
		desconectarPersonaje(personajePorNivel);
	}

	if(muertePorSenial){
		devolverRecursosPorMuerte(personajePorNivel->socketPlataforma);
		desconectarPersonaje(personajePorNivel);
	}


}
bool personajeEstaMuerto(bool murioPersonaje){
	//si esta muerto por alguna señal o porque se quedo sin vidas
	return (murioPersonaje || muertePorSenial || personaje.vidas<=0);
}

bool conseguiRecurso(personajeIndividual_t personajePorNivel){
	return ((personajePorNivel.posY == personajePorNivel.posRecursoY) && (personajePorNivel.posX == personajePorNivel.posRecursoX));
}

void moverAlPersonaje(personajeIndividual_t* personajePorNivel){
	tDirMovimiento  mov;

	calcularYEnviarMovimiento(*personajePorNivel);

	//Actualizo mi posición y de acuerdo a eso armo mensaje de TURNO
	actualizaPosicion(&mov, personajePorNivel);

	/*
	 Para que se hace esto?? hace falta avisarle a la plataforma que no alcanzo el recurso?
	 Si lo alcanzo le aviso?
	 */

	//El personaje no llego al recurso
	tPaquete pkgFinTurno;
	pkgFinTurno.type   = P_FIN_TURNO;
	pkgFinTurno.length = 0;
	enviarPaquete(personajePorNivel->socketPlataforma, &pkgFinTurno, logger, "Fin de turno del personaje");

}

void calcularYEnviarMovimiento(personajeIndividual_t personajePorNivel){
	tMensaje tipoMensaje;
	tMovimientoPers movimientoAEnviar;
	movimientoAEnviar.simbolo=personaje.simbolo;
	movimientoAEnviar.direccion=calculaMovimiento(personajePorNivel);

	tPaquete pkgMovimiento;
	//serializarMovimientoPers(tMensaje tipoMensaje, tMovimientoPers movimientoPers, tPaquete* pPaquete)
	serializarMovimientoPers(P_MOVIMIENTO, movimientoAEnviar, &pkgMovimiento);

	enviarPaquete(personajePorNivel.socketPlataforma, &pkgMovimiento, logger, "Envio pedido de movimiento del personaje");

	char* sPayload;
	recibirPaquete(personajePorNivel.socketPlataforma, &tipoMensaje, &sPayload, logger, "Recibo confirmacion del movimiento");

	switch(tipoMensaje){
		case PL_MUERTO_POR_ENEMIGO:{
			log_info(logger, "El personaje se murio por enemigos");
			restarVida();
			break;
		}
		case PL_MUERTO_POR_DEADLOCK:{
			log_info(logger, "El personaje se murio por deadlock");
			restarVida();
			break;
		}
		case PL_CONFIRMACION_MOV:{
			log_info(logger, "Movimiento confirmado");
			break;
		}
		default: {
			log_error(logger, "Llego un mensaje (tipoMensaje: %d) cuando debia llegar PL_CONFIRMACION_MOV", tipoMensaje);
			exit(EXIT_FAILURE);
			break;
		}
	}

}

void recibirMensajeTurno(int socketPlataforma){
	tMensaje tipoMensaje;
	char* sPayload;
	recibirPaquete(socketPlataforma, &tipoMensaje, &sPayload, logger, "Se le otorgo un turno al personaje");

	if (tipoMensaje != PL_OTORGA_TURNO){
		log_error(logger, "Llego un mensaje (tipoMensaje: %d) cuando debia llegar PL_OTORGA_TURNO", tipoMensaje);
		exit(EXIT_FAILURE);
	}

}


void pedirPosicionRecurso(personajeIndividual_t* personajePorNivel, char* recurso){

	tMensaje tipoMensaje;
	tPregPosicion solicitudRecurso;
	solicitudRecurso.simbolo=personaje.simbolo;
	solicitudRecurso.recurso= *recurso;

	tPaquete pkgSolicitudRecurso;
	serializarPregPosicion(PL_SOLICITUD_RECURSO, solicitudRecurso, &pkgSolicitudRecurso);

	enviarPaquete(personajePorNivel->socketPlataforma, &pkgSolicitudRecurso, logger, "Solicito la posicion de un recurso");

	char* sPayload;
	recibirPaquete(personajePorNivel->socketPlataforma, &tipoMensaje, &sPayload, logger, "Recibo posicion del recurso");

	if (tipoMensaje != PL_POS_RECURSO){
		log_error(logger, "Llego un mensaje (tipoMensaje: %d) cuando debia llegar PL_POS_RECURSO", tipoMensaje);
		exit(EXIT_FAILURE);
	}

	tRtaPosicion* rtaSolicitudRecurso;
	rtaSolicitudRecurso = deserializarRtaPosicion(sPayload);

	personajePorNivel->posRecursoX = rtaSolicitudRecurso->posX;
	personajePorNivel->posRecursoY = rtaSolicitudRecurso->posY;

}

bool estaMuerto(tMensaje tipoMensaje, bool *murioPj){
	if(tipoMensaje == PL_MUERTO_POR_DEADLOCK)
		return (*murioPj = true);
	if(tipoMensaje == PL_MUERTO_POR_ENEMIGO)
		return (*murioPj = true);
	return (*murioPj =false);
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

	//Recibo un aviso de que existe o no el nivel
	if (tipoMensaje == PL_NIVEL_INEXISTENTE || tipoMensaje == PL_PERSONAJE_REPETIDO){
		reintentarHandshake(personajePorNivel->socketPlataforma, &pkgHandshake);
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
	log_debug(logger, "Cierro conexion con la plataforma");
}


void devolverRecursosPorFinNivel(int socketPlataforma) {
	tPaquete pkgDevolverRecursos;
	pkgDevolverRecursos.type   = P_DESCONECTARSE_FINALIZADO;
	pkgDevolverRecursos.length = 0;
	enviarPaquete(socketPlataforma, &pkgDevolverRecursos, logger, "Se liberan los recursos del personaje por terminar el nivel");

	tMensaje tipoMensaje;
	char* sPayload;
	recibirPaquete(socketPlataforma, &tipoMensaje, &sPayload, logger, "Recibo confirmacion de salida y liberacion de recursos");

	if (tipoMensaje != PL_CONFIRMACION_ELIMINACION) {
		log_error(logger, "Tipo de mensaje incorrecto, se esperaba PL_CONFIRMACION_ELIMINACION y llego %d", tipoMensaje);
		exit(EXIT_FAILURE);
	}

	log_trace(logger, "Los recursos fueron liberados por conclusion del nivel");

}

void devolverRecursosPorMuerte(int socketPlataforma){
	tPaquete pkgDevolverRecursos;
	pkgDevolverRecursos.type   = P_DESCONECTARSE_MUERTE;
	pkgDevolverRecursos.length = 0;
	enviarPaquete(socketPlataforma, &pkgDevolverRecursos, logger, "Se liberan los recursos del personaje por muerte del personaje");

	tMensaje tipoMensaje;
	char* sPayload;
	recibirPaquete(socketPlataforma, &tipoMensaje, &sPayload, logger, "Recibo confirmacion de salida y liberacion de recursos por muerte del personaje");


	if (tipoMensaje != PL_CONFIRMACION_ELIMINACION) {
		log_error(logger, "Tipo de mensaje incorrecto, se esperaba PL_CONFIRMACION_ELIMINACION y llego %d", tipoMensaje);
		exit(EXIT_FAILURE);
	}

	log_trace(logger, "Los recursos fueron liberados por la muerte del personaje ");

}

//Seniales
bool validarSenial(bool *murioPersonaje){
	if(muertePorSenial){
		return (*murioPersonaje = true);
	}
	else
		return (*murioPersonaje = false);
}

void morirSenial() {
	muertePorSenial=true;
}

int calculaMovimiento(personajeIndividual_t personajePorNivel){

	if (conseguiRecurso(personajePorNivel)){
		return -1;
	}
	while (1) {
		r = rand() % 2;
		if (r) {

			//Sobre el eje x

			if (r && personajePorNivel.posX < personajePorNivel.posRecursoX)
				return derecha;
			else if (personajePorNivel.posX > personajePorNivel.posRecursoX)
				return izquierda;
		} else {

			// Sobre el eje y

			if (personajePorNivel.posY < personajePorNivel.posRecursoY)
				return abajo;
			else if (personajePorNivel.posY > personajePorNivel.posRecursoY)
				return arriba;
		}
	}

	return -700;
}


void actualizaPosicion(tDirMovimiento* movimiento, personajeIndividual_t *personajePorNivel) {
	// Actualiza las variables posicion del personaje a partir del movimiento que recibe por parametro.
	// El eje Y es alreves, por eso para ir para arriba hay que restar en el eje y.
	switch (*movimiento) {
		case arriba:
			(personajePorNivel->posY)--;
			break;
		case abajo:
			(personajePorNivel->posY)++;
			break;
		case derecha:
			(personajePorNivel->posX)++;
			break;
		case izquierda:
			(personajePorNivel->posY)--;
			break;
	}

}

void restarVida(){
	char n;
	pthread_mutex_lock(&semModificadorDeVidas);
	personaje.vidas--;

	personajeIndividual_t* unPersonaje;
	if (personaje.vidas <= 0) {
		int i;

		/* matar a todos los threads */
		for (i = 0; i < cantidadNiveles; i++){
			pthread_cancel(hilosNiv->thread);

			unPersonaje = dictionary_get(listaPersonajePorNiveles, hilosNiv->nivel.nomNivel);
			if (unPersonaje->socketPlataforma!=0)
				desconectarPersonaje(unPersonaje);
		}

		printf("\n ¿Desea volver a intentar? (Y/N) ");
		n = getchar();
		while( (n != 'N') | (n != 'Y') ){
			n = getchar();
			printf("No entiendo ese comando");
			printf("\n ¿Desea volver a intentar? (Y/N) ");
		}
		if (n == 'Y') continuar = true;
		if (n == 'N') continuar = false;
	}
	pthread_mutex_unlock(&semModificadorDeVidas);

}


