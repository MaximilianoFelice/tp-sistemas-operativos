/*
 * Sistemas Operativos - Super Mario Proc RELOADED.
 * Grupo       : C o no ser.
 * Nombre      : personaje.c.
 * Descripcion : Este archivo contiene la implementacion de las
 * funciones usadas por el personaje.
 */

#include "personaje.h"

pthread_mutex_t semPersonaje;

personajeGlobal_t personaje;
t_config *configPersonaje;
t_log *logger;
char * ip_plataforma;
char * puerto_orq;

int r = 0;
bool muertePorSenial=false; //Cuando se activa, se cierra t0d0, no importan las vidas  (todo revisar)
bool inicializeVidas = false;

int main(int argc, char*argv[]) {

	pthread_mutex_init(&semPersonaje, NULL ); // TODO Usar para algo.

	if (signal(SIGINT, morirSenial) == SIG_ERR) {
		log_error(logger, "Error en el manejo de la senal de muerte del personaje.\n", stderr);
		return exit(EXIT_FAILURE);
	}
	if (signal(SIGTERM, restarVidas) == SIG_ERR) {
		log_error(logger, "Error en el manejo de la senal de restar vidas del personaje.\n", stderr);
		return exit(EXIT_FAILURE);
	}
	if (signal(SIGUSR1, aumentarVidas) == SIG_ERR) {
		log_error(logger, "Error en el manejo de la senal de de aumentar vidas del personaje.\n", stderr);
		return exit(EXIT_FAILURE);
	}


	// Inicializa el log.
	logger = logInit(argv, "PERSONAJE");


	// Creamos el archivo de Configuración
	cargarArchivoConfiguracion(argv[1]);
	int cantidadNiveles =list_size(personaje.listaNiveles);
	threadNivel_t *hilosNiv;
	hilosNiv = calloc(cantidadNiveles, sizeof(threadNivel_t));

	int i=0;
	int indiceNivel;
	for( indiceNivel = 0;  indiceNivel < cantidadNiveles; indiceNivel ++) {
		//creo estructura del nivel que va a jugar cada hilo
		hilosNiv[i].nivel.nomNivel = ((nivel_t*) list_get_data(personaje.listaNiveles,indiceNivel))->nomNivel;
		hilosNiv[i].nivel.Objetivos = ((nivel_t*) list_get_data(personaje.listaNiveles,	indiceNivel))->Objetivos;
		hilosNiv[i].nivel.num_of_thread = indiceNivel;

		//Tiro el hilo para jugar de cada nivel
		if (pthread_create(&hilosNiv[i].thread, NULL, jugar, (void *) &hilosNiv[i].nivel)) {
			log_error(logger, "pthread_create: %s", strerror(errno));
			exit(EXIT_FAILURE);
		}

		i++;
	}

	char *join_return;
	for (i = 0; i < cantidadNiveles; i++) {
		pthread_join(hilosNiv[i].thread, (void**)&join_return);
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

	pthread_mutex_destroy(&semPersonaje);

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

	bool finalice = false;
	bool murioPersonaje = false;

	while (1) {//FIXME CAMBIAR este while, para adaptarlo a la ejecucion de koopa

		if (!inicializeVidas) { //Si no inicialice vidas, las inicializo
			personaje.vidas=personaje.vidasMaximas;
			inicializeVidas = true; //Esta variable solo se pone en false cuando el chaboncito se queda sin vidas y quiere reiniciar
			//reiniciar=false;
		}

		//Setea los flags del inicio
		finalice = false;
		murioPersonaje = false;

		while (personaje.vidas  > 0) {

			murioPersonaje = false;
			finalice = false;

			log_info(logger, "Vidas de %c: %d", personaje.simbolo, personaje.vidas);

			// todo Hay que chequear que al morir el personaje realize las acciones necesarias.
			personajePorNivel.socketPlataforma= connectToServer(ip_plataforma, atoi(puerto_orq), logger);

			handshake_plataforma(&personajePorNivel);


			// Por cada objetivo del nivel,
			for (personajePorNivel.objetivoActual=0; (personajePorNivel.objetivoActual < list_size(personajePorNivel.nivelQueJuego->Objetivos)) && (!personajeEstaMuerto(murioPersonaje)); personajePorNivel.objetivoActual++) {

				murioPersonaje = false;

				//agarra un recurso de la lista de objetivos del nivel
				char* recurso = (char*) list_get_data(personajePorNivel.nivelQueJuego->Objetivos,	personajePorNivel.objetivoActual);

				pedirPosicionRecurso(&personajePorNivel, recurso);

				while (!conseguiRecurso(personajePorNivel)) {

					//FIXME modificar la funcion de muerte por senal y que lo mate y llame a una funcion de muerte que pare to do y pregunte si quiere volver a jugar
					if (validarSenial(&murioPersonaje) || personaje.vidas<=0)
						break;

					//Espera que el planificador le de el turno
					recibirMensajeTurno(personajePorNivel.socketPlataforma);

					//fixme ver si son bloqueantes y si los necesito
					if (validarSenial(&murioPersonaje) || personaje.vidas<=0)
						break;

					log_info(logger, "Habemus turno");

					//El personaje se mueve
					moverAlPersonaje(&personajePorNivel);

				}

			} //Fin de for de objetivos

			manejarDesconexiones(personajePorNivel, murioPersonaje, &finalice);


			if(muertePorSenial || finalice)
				break;

		} //Fin del while(vidas>0)

		//TODO borrar cuando sepamos usar bien las senales
		if (finalice || muertePorSenial || personaje.vidas<=0)
			break;

	} //Fin de while(1) de control de reinicio del personaje TODO Borrar

	//Aqui siempre va a terminar porque: termino su nivel bien; se cerro el proceso por señal; se acabaron sus vidas y no quiere reiniciar

	if (personaje.vidas<=0) {
		char *exit_return;
		exit_return = strdup("se ha quedado sin vidas y murio :'(");
		pthread_exit((void *)exit_return);
	}
	if (murioPersonaje && muertePorSenial) {// TODO Verificar si se debe evaluar murioPersonaje, porque muertePorSenial ya es condicion suficiente
		char *exit_return;
		exit_return = strdup("ha terminado por senial SIGINT");
		pthread_exit((void *)exit_return);
	}

	char * exit_return = strdup("ha finalizado su plan de niveles correctamente");
	pthread_exit((void *)exit_return);


}

void manejarDesconexiones(personajeIndividual_t personajePorNivel, bool murioPersonaje, bool* finalice){
	if (!murioPersonaje) {
		*finalice = true;
		devolverRecursosPorFinNivel(personajePorNivel.socketPlataforma);
		cerrarConexiones(&personajePorNivel.socketPlataforma);
	}

	if(personaje.vidas <= 0){
		devolverRecursosPorMuerte(personajePorNivel.socketPlataforma);
		cerrarConexiones(&personajePorNivel.socketPlataforma);
	}

	//FIXME DEBERIA restarle una vida sola en lugar de matarlo completamente ?? REVISAR
	if(muertePorSenial){
		devolverRecursosPorMuerte(personajePorNivel.socketPlataforma);
		cerrarConexiones(&personajePorNivel.socketPlataforma);
	}

	if(murioPersonaje){
		if (personaje.vidas<=0) {
			//Si me quede sin vidas armo un mensaje especial para que el planificador libere memoria
			//TODO preguntar si tiene q reiniciar o no - contemplar estas posibilidades en la funcion de muerte
			//TODO si dice q no = > finalice=true

			devolverRecursosPorMuerte(personajePorNivel.socketPlataforma);
		}
		personaje.vidas--;
		log_debug(logger, "Me han matado :/");
		cerrarConexiones(&personajePorNivel.socketPlataforma);
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

	if (tipoMensaje != PL_CONFIRMACION_MOV){

		//todo verificar excepcions
		//TODO VERIFICAR si chequea aca si esta muerto por un enemigo y que pasa cuando sale del while y entra en el for
		/*if(estaMuerto(msjPlan.detail, &murioPersonaje))
			break;*/

		log_error(logger, "Llego un mensaje (tipoMensaje: %d) cuando debia llegar PL_CONFIRMACION_MOV", tipoMensaje);
		exit(EXIT_FAILURE);
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

	tRtaPosicion* rtaSolicitudRecurso;
	rtaSolicitudRecurso = deserializarRtaPosicion(sPayload);

	if (tipoMensaje != PL_CONFIRMACION_MOV){

		log_error(logger, "Llego un mensaje (tipoMensaje: %d) cuando debia llegar PL_CONFIRMACION_MOV", tipoMensaje);
		exit(EXIT_FAILURE);
	}


	personajePorNivel->posRecursoX = rtaSolicitudRecurso->posX;
	personajePorNivel->posRecursoY = rtaSolicitudRecurso->posY;

}

bool estaMuerto(tMensaje tipoMensaje, bool *murioPj){
	//FIXME no deberia devolver nada porque ya pisa el valor
	//no se usa
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
	strcpy(handshakePers.nombreNivel, "unNombre");
	/* Se crea el paquete */
	tPaquete pkgHandshake;
	serializarHandshakePers(P_HANDSHAKE, handshakePers, &pkgHandshake);

	enviarPaquete(personajePorNivel->socketPlataforma, &pkgHandshake, logger, "Se envia saludo a la plataforma");

	char* sPayload;
	recibirPaquete(personajePorNivel->socketPlataforma, &tipoMensaje, &sPayload, logger, "Recibo si existe el nivel solicitado");

	//Recibo un aviso de que existe o no el nivel
	if (tipoMensaje == PL_NIVEL_INEXISTENTE){
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
/*
 *
 * lo comento porque todavia no se uso
 *
 *
 *
void morir(char* causaMuerte, personajeIndividual_t personajePorNivel) {
	log_info(logger, "%s murio por: %s", personaje.nombre, causaMuerte);
	*personajePorNivel.objetivoActual=1000;
	personaje.vidas--;
}*/


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

void aumentarVidas() {
	personaje.vidas ++;
	log_info(logger, "Vidas del personaje %c: %d", personaje.simbolo, personaje.vidas);
}

void restarVidas() {
	personaje.vidas--;
	log_info(logger, "Vidas del personaje %c: %d", personaje.simbolo, personaje.vidas);
}
//Seniales

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
/*
 *
  COSAS PARA AGREGAR

1- Controlar la modificacion de vidas con semaforos
2- Si el personaje pierde todas las vidas debe:
 	*lockear al resto de los threads y preguntar si quiere reiniciar o no.
	*Los hilos bloqueados deberian mandar un mensaje de SALIR + hilos sin vidas y lockearse esperando ser desbloqueados por el hilo que realizo la consulta.
Si no se quiere reiniciar, debe mandar SALIR + MURIO_ENEMIGOS
 * */

