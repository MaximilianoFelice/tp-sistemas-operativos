/*
 * Sistemas Operativos - Super Mario Proc RELOADED.
 * Grupo       : C o no ser.
 * Nombre      : plataforma.c.
 * Descripcion : Este archivo contiene la implementacion de las
 * funciones usadas por la plataforma.
 */

#include "plataforma.h"

#include <stdbool.h>

pthread_mutex_t semNivel;
pthread_mutex_t mtxlNiveles;
t_list 	 *listaNiveles;
t_list *lPlanificadores;
t_config *configPlataforma;
t_log *logger;
unsigned short usPuerto;
int nroConexiones;
fd_set setSocketsOrquestador;
int iSocketMaximoOrquestador;
char *pathKoopa;
char *pathScript;

t_list* personajes_jugando;

/*
 * Funciones privadas
 */
tPersonaje* desbloquearPersonaje(t_list* lBloqueados, tSimbolo recurso);
tPersonaje *sacarPersonajeDeListas(tNivel *pNivel, int iSocket, bool *bloqueado);
char *getRecursosNoAsignados(t_list *recursos, char *recursosNoAsignados, int *lengthRecursos, bool bloqueado);
void waitPersonajes(tNivel *pNivel, tPersonaje **personajeActual);

/*
 * Funciones privadas plataforma y para la ejecucion de Koopa
 */
void add_new_personaje_in_plataforma(tSimbolo simbolo);
void verificarKoopa(char *sPayload);
int executeKoopa(char *koopaPath, char *scriptPath);
//void orquestadorTerminaJuego();
bool nivelVacio(tNivel *unNivel);
void cerrarTodoSignal();
void cerrarTodo();

/*
 * Funciones privadas orquestador
 */
int conexionNivel(int iSocketComunicacion, char* sPayload, fd_set* pSetSocketsOrquestador);
int conexionPersonaje(int iSocketComunicacion, fd_set* socketsOrquestador, char* sPayload);
bool avisoConexionANivel(int sockNivel,char *sPayload, tSimbolo simbolo);
void sendConnectionFail(int sockPersonaje, tMensaje typeMsj, char *msjInfo);


/*
 * Funciones privadas planificador
 */
void imprimirLista(tNivel *pNivel, tPersonaje *pPersonaje);
int seleccionarJugador(tPersonaje** pPersonaje, tNivel* nivel);
tPersonaje* planificacionSRDF(tNivel *nivel);
void obtenerDistanciaFaltante(tPersonaje *pPersonajeActual, char * sPayload);
void enviarTurno(tNivel *pNivel, tPersonaje *pPersonaje, int delay);
void solicitudRecursoPersonaje(int iSocketConexion, char *sPayload, tNivel *pNivel, tPersonaje **pPersonajeActual);
void movimientoPersonaje(int iSocketConexion, char* sPayload, tNivel *pNivel, tPersonaje** pPersonajeActual);
void personajeSolicitaPosicionRecurso(tNivel *pNivel, tPersonaje *pPersonajeActual, int iSocketConexion, char* sPayload, t_log* logger);
void entregoPosicionRecursoAlPersonaje(tNivel *pNivel, tPersonaje *pPersonajeActual, int iSocketConexion, char* sPayload, t_log* logger);
int actualizacionCriteriosNivel(int iSocketConexion, char* sPayload, tNivel* pNivel, tPersonaje *pPersonajeActual);
void recepcionRecurso(tNivel *pNivel, tPersonaje **pPersonajeActual, char *sPayload);
void recepcionBloqueado(tNivel *pNivel, tPersonaje **pPersonajeActual, char *sPayload);
void confirmarMovimiento(tNivel *nivel, tPersonaje *pPersonajeActual);
void muertePorEnemigoPersonaje(tNivel *pNivel, tPersonaje** pPersonaje, int iSocketConexion, char* sPayload);
int avisarAlPersonajeDeMuerte(int socketPersonajeMuerto, tSimbolo simbolo, tMensaje tipoDeMuerte);
void muertePorDeadlockPersonaje(tNivel *pNivel, char *sPayload);
int desconectar(tNivel *pNivel, tPersonaje **pPersonajeActual, int iSocketConexion);
int desconectarNivel(tNivel *pNivel);
int desconectarPersonaje(tNivel *pNivel, tPersonaje **pPersonajeActual, int iSocketConexion);
int liberarRecursosYDesbloquearPersonajes(tNivel *pNivel, tPersonaje *pPersonaje, bool bloqueado);
void enviarPersonajesDesbloqueadosAlNivel(tNivel *pNivel, tSimbolo simboloPersonaje, tMensaje tipoMensaje, char *personajesDesbloqueados, int cantidad);
void enviarRecursosLiberadosAlNivel(tNivel *pNivel, tSimbolo simboloPersonaje, tMensaje, char *personajesDesbloqueados, int cantidad);
void liberarRecursos(tPersonaje *pPersMuerto, tNivel *pNivel, bool bloqueado, char *personajesDeadloqueados, int *cantDesbloqueados, char *recursosNoAsignados, int *lengthRecursos);
bool coordinarAccesoMultiplesPersonajes(tPersonaje *personajeActual, int socketConexion, bool valor);
bool esElPersonajeQueTieneElTurno(int socketActual, int socketConexion);

/*
 * PLATAFORMA
 */

//t_estado_personaje* crear_estado_personaje(tSimbolo simbolo) {
//        t_estado_personaje* estado = malloc(sizeof(t_estado_personaje));
//        estado->estado = FALSE;
//        estado->simbolo = simbolo;
//        return estado;
//}

//void add_to_character_list(tSimbolo simbolo){
//
//                bool _mismoSimbolo(t_estado_personaje personaje){
//                        tSimbolo sim;
//                        memcpy(&sim, &(personaje.simbolo), sizeof(tSimbolo));
//                        if (personaje.simbolo == simbolo) return TRUE;
//                        else return FALSE;
//                }
//
//        if (!list_any_satisfy(personajes_jugando, (void*) _mismoSimbolo)){
//                t_estado_personaje* estado = crear_estado_personaje(string_from_format("%d", simbolo));
//                t_estado_personaje* estado = crear_estado_personaje(simbolo);
//                list_add(personajes_jugando, estado);
//        }
//
//}

//void setAsFinished(char* sPayload){
//        tSimbolo simbolo;
//        memcpy(&simbolo,sPayload,sizeof(tSimbolo));
//        t_estado_personaje *pj;
////        log_debug(logger, "ME LLEGO EL SIMBOLO %d", simbolo);
//
//        bool _mismoSimbolo(t_estado_personaje personaje){
//                if (personaje.simbolo == simbolo) return TRUE;
//                else return FALSE;
//        }
//
//        pj = list_find(personajes_jugando, (void*) _mismoSimbolo);
//        pj->estado = TRUE;
//}

int main(int argc, char*argv[]) {

	signal(SIGINT, cerrarTodoSignal);

	if (argc <= 1) {
		printf("[ERROR] Se debe pasar como parametro el archivo de configuracion.\n");
		exit(EXIT_FAILURE);
	}

	int thResult;
	pthread_t thOrquestador;

	// Creamos el archivo de configuracion
	configPlataforma = config_try_create(argv[1], "puerto,koopa,script");

	// Obtenemos el puerto
	usPuerto   = config_get_int_value(configPlataforma, "puerto");

	// Obtenemos el path donde va a estar koopa
	pathKoopa  = config_get_string_value(configPlataforma, "koopa");

	// Obtenemos el path donde va a estar el script para el filesystem
	pathScript = config_get_string_value(configPlataforma, "script");

	logger = logInit(argv, "PLATAFORMA");

	// Inicializo el semaforo
	pthread_mutex_init(&semNivel, NULL);
	pthread_mutex_init(&mtxlNiveles, NULL);

	// Inicializa la lista de personajes
	personajes_jugando = list_create();

	thResult = pthread_create(&thOrquestador, NULL, orquestador, (void *) &usPuerto);

	if (thResult != 0) {
		log_error(logger, "No se pudo crear el hilo orquestador.");
		exit(EXIT_FAILURE);
	}

	pthread_join(thOrquestador, NULL);

	log_destroy(logger);

	exit(EXIT_SUCCESS);
}

void add_new_personaje_in_plataforma(tSimbolo simbolo){
	int i;
	bool encontrado=false;
	for(i=0; i < list_size(personajes_jugando); i++){
		t_estado_personaje * personaje = list_get(personajes_jugando, i);
		if(personaje->simbolo == simbolo){
			encontrado =true;
			break;
		}
	}
	if(!encontrado){
		t_estado_personaje personaje;
		personaje.simbolo = simbolo;
		personaje.estado = false;
		list_add_new(personajes_jugando, &personaje, sizeof(t_estado_personaje));
	}
}

void verificarKoopa(char *sPayload){
	tSimbolo *simbolo = deserializarSimbolo(sPayload);
	free(sPayload);

	log_info(logger, "El personaje %c finalizo su ejecucion", *simbolo);
	bool _search_symbol(t_estado_personaje *personaje){
		return(personaje->simbolo == *simbolo);
	}
	t_estado_personaje *personaje = list_remove_by_condition(personajes_jugando, (void *)_search_symbol);
	free(personaje);

	if(list_size(personajes_jugando) == 0){
		log_debug(logger, "No hay tipitos jugando entonces ejecuto koopa y cierro todo");
		cerrarTodo();
		executeKoopa(pathKoopa, pathScript);
		exit(EXIT_SUCCESS);
	}

}

int executeKoopa(char *koopaPath, char *scriptPath) {

	// parametros para llamar al execve
	char * arg2[] = {"koopa", "koopa.conf", NULL}; //parámetros (archivo de confg)
	char * arg3[] = {"TERM=xterm",NULL};

	// llamo a koopa
	int ejecKoopa = execve(koopaPath, arg2, arg3);

	if (ejecKoopa < 0){ // algo salió mal =(
		log_error(logger, "No se pudo ejecutar Koopa - error: %d", ejecKoopa);
		return EXIT_FAILURE;

	} else {
		return EXIT_SUCCESS;
	}
}

//Con esto mato a todos los niveles... y de paso tmb a los personajes(que deberia estar cerrados)
void cerrarTodoSignal() {
	printf("\n");
	system("killall -SIGKILL personaje >/dev/null 2>&1");
	system("killall -SIGINT nivel >/dev/null 2>&1");
	printf("Process Killed.\n");
	exit(EXIT_FAILURE);
}

void cerrarTodo() {
	system("killall -SIGINT nivel > /dev/null 2>&1");
	system("killall -SIGINT personaje > /dev/null 2>&1");
	log_trace(logger, "Proceso Finalizado.");
}

void destroyNivel(tNivel *pNivel){
	list_destroy(pNivel->lBloqueados);
	queue_destroy(pNivel->cListos);
	free(pNivel->nombre);
	pthread_cond_destroy(&pNivel->hayPersonajes);
	free(pNivel);
}

void destroyPlanificador(pthread_t *pPlanificador){
	pthread_exit(NULL);
	free(pPlanificador);
}

/*
 * ORQUESTADOR
 */

void *orquestador(void *vPuerto) {

	unsigned short usPuerto;
	usPuerto = *(unsigned short*)vPuerto;

	// Creo la lista de niveles y planificadores
	lPlanificadores = list_create();
	listaNiveles    = list_create();

	// Definicion de variables para los sockets
	FD_ZERO(&setSocketsOrquestador);
	int iSocketEscucha, iSocketComunicacion;

	// Inicializacion de sockets
	iSocketEscucha = crearSocketEscucha(usPuerto, logger);
	FD_SET(iSocketEscucha, &setSocketsOrquestador);
	iSocketMaximoOrquestador = iSocketEscucha;

	tMensaje tipoMensaje;
	char * sPayload;

	while (1) {
		iSocketComunicacion = getConnection(&setSocketsOrquestador, &iSocketMaximoOrquestador, iSocketEscucha, &tipoMensaje, &sPayload, logger);

		if (iSocketComunicacion != -1) {

			switch (tipoMensaje) {
			case N_HANDSHAKE: // Un nuevo nivel se conecta
				conexionNivel(iSocketComunicacion, sPayload, &setSocketsOrquestador);
				break;

			case P_HANDSHAKE:
				conexionPersonaje(iSocketComunicacion, &setSocketsOrquestador, sPayload);
				break;

			case P_FIN_PLAN_NIVELES:
				verificarKoopa(sPayload);
				//setAsFinished(sPayload);
				//orquestadorTerminaJuego();
				break;

			case DESCONEXION:
				break;

			default:
				break;
			}
		}
	}

	pthread_exit(NULL);
}

//void orquestadorTerminaJuego() {
//	int indiceNivel;
//	int cantidadNiveles;
//	tNivel *nivelLevantador;
//
//	pthread_mutex_lock(&mtxlNiveles);
//	log_debug(logger, "Verificando niveles para ejecucion de Koopa...");
//	cantidadNiveles = list_size(listaNiveles);
//	bool noHayPersonajes = (cantidadNiveles == 0 ? false : true);
//	nroConexiones--; //Me llego una desconexion, resto.
//	log_debug(logger, "Cantidad de conexiones = %d", nroConexiones);
//	log_debug(logger, "Cantidad de niveles    = %d", cantidadNiveles);
//
//	//Reviso los niveles
//	for (indiceNivel=0; indiceNivel < cantidadNiveles; indiceNivel++) {
//		nivelLevantador = list_get(listaNiveles, indiceNivel);
//		if (!nivelVacio(nivelLevantador)) {
//			noHayPersonajes = false;
//			break;
//		}
//	}
//
//	if (noHayPersonajes && soloQuedanNiveles()) {
//		log_debug(logger, "No hay tipitos jugando entonces ejecuto koopa y cierro todo");
//		cerrarTodo();
//		executeKoopa(pathKoopa, pathScript);
//		pthread_mutex_unlock(&mtxlNiveles);
//		exit(EXIT_SUCCESS);
//	}
//	pthread_mutex_unlock(&mtxlNiveles);
//	bool _termino_plan(t_estado_personaje* estado) {
//		return estado->estado == TRUE;
//	}
//	if (list_all_satisfy(personajes_jugando, (void*) _termino_plan)) {
//		cerrarTodo();
//		executeKoopa(pathKoopa, pathScript);
//	}
//		pthread_mutex_unlock(&mtxlNiveles);
//		exit(EXIT_FAILURE);
//	}
//	pthread_mutex_unlock(&mtxlNiveles);
//}


bool nivelVacio(tNivel* nivel) {
	return ((list_size(nivel->cListos->elements)==0) && (list_size(nivel->lBloqueados) == 0));
}

int conexionNivel(int iSocketComunicacion, char* sPayload, fd_set* pSetSocketsOrquestador) {

	int iIndiceNivel;

	pthread_mutex_lock(&mtxlNiveles);
	iIndiceNivel = existeNivel(listaNiveles, sPayload);
	pthread_mutex_unlock(&mtxlNiveles);

	if (iIndiceNivel >= 0) {
		tPaquete pkgNivelRepetido;
		pkgNivelRepetido.type   = PL_NIVEL_YA_EXISTENTE;
		pkgNivelRepetido.length = 0;
		enviarPaquete(iSocketComunicacion, &pkgNivelRepetido, logger, "Ya se encuentra conectado al orquestador un nivel con el mismo nombre");
		free(sPayload);
		return EXIT_FAILURE;
	}

	pthread_t *pPlanificador;
	tMensaje tipoMensaje;
	tInfoNivel *pInfoNivel;

	tNivel *pNivelNuevo = (tNivel *)    malloc(sizeof(tNivel));
	pPlanificador 	    = (pthread_t *) malloc(sizeof(pthread_t));
	log_debug(logger, "Se conecto el nivel %s", sPayload);

	char* sNombreNivel = (char *)malloc(strlen(sPayload) + 1);
	strcpy(sNombreNivel, sPayload);
	free(sPayload);

	tPaquete pkgHandshake;
	pkgHandshake.type   = PL_HANDSHAKE;
	pkgHandshake.length = 0;
	enviarPaquete(iSocketComunicacion, &pkgHandshake, logger, "Handshake Plataforma");

	// Ahora debo esperar a que me llegue la informacion de planificacion.
	recibirPaquete(iSocketComunicacion, &tipoMensaje, &sPayload, logger, "Recibe mensaje informacion del nivel");

	pInfoNivel = deserializarInfoNivel(sPayload);
	free(sPayload);

	// Validacion de que el nivel me envia informacion correcta
	if (tipoMensaje == N_DATOS) {

		crearNivel(listaNiveles, pNivelNuevo, iSocketComunicacion, sNombreNivel, pInfoNivel);
		crearHiloPlanificador(pPlanificador, pNivelNuevo);
		delegarConexion(&pNivelNuevo->masterfds, pSetSocketsOrquestador, iSocketComunicacion, &pNivelNuevo->maxSock);
		log_debug(logger, "Nuevo planificador del nivel: '%s' y planifica con: %i", pNivelNuevo->nombre, pNivelNuevo->algoritmo);

		free(sNombreNivel);
		free(pInfoNivel);
		return EXIT_SUCCESS;

	} else {
		log_error(logger,"Tipo de mensaje incorrecto: se esperaba datos del nivel");
		return EXIT_FAILURE;
	}
}

int conexionPersonaje(int iSocketComunicacion, fd_set* socketsOrquestador, char* sPayload) {
	tHandshakePers* pHandshakePers;
	pHandshakePers = deserializarHandshakePers(sPayload);
	free(sPayload);
	int iIndiceNivel;

	log_info(logger, "Se conectó el personaje %c pidiendo el nivel '%s'", pHandshakePers->simbolo, pHandshakePers->nombreNivel);

	pthread_mutex_lock(&mtxlNiveles);
	iIndiceNivel = existeNivel(listaNiveles, pHandshakePers->nombreNivel);

	if (iIndiceNivel >= 0) {
		tNivel *pNivelPedido;
		pNivelPedido = list_get_data(listaNiveles, iIndiceNivel);
		pthread_mutex_unlock(&mtxlNiveles);

		if (pNivelPedido == NULL) {
			log_error(logger, "Saco mal el nivel: Puntero en NULL");
			sendConnectionFail(iSocketComunicacion, PL_NIVEL_INEXISTENTE, "No se encontro el nivel pedido");
		}

//		bool rta_nivel;
		avisoConexionANivel(pNivelPedido->socket, sPayload, pHandshakePers->simbolo);

//		if (rta_nivel) {
		add_new_personaje_in_plataforma(pHandshakePers->simbolo);

		agregarPersonaje(pNivelPedido, pHandshakePers->simbolo, iSocketComunicacion);

		free(pHandshakePers->nombreNivel);
		free(pHandshakePers);

		delegarConexion(&pNivelPedido->masterfds, socketsOrquestador, iSocketComunicacion, &pNivelPedido->maxSock);

		pthread_cond_signal(&(pNivelPedido->hayPersonajes));

		tPaquete pkgHandshake;
		pkgHandshake.type   = PL_HANDSHAKE;
		pkgHandshake.length = 0;
		// Le contesto el handshake
		enviarPaquete(iSocketComunicacion, &pkgHandshake, logger, "Handshake de la plataforma al personaje");

		return EXIT_SUCCESS;

//		}
//		else {
//			log_error(logger, "El personaje ya esta jugando actualmente en ese nivel");
//			sendConnectionFail(iSocketComunicacion, PL_PERSONAJE_REPETIDO, "El personaje ya esta jugando ese nivel");
//
//			free(pHandshakePers->nombreNivel);
//			free(pHandshakePers);
//			return EXIT_FAILURE;
//		}

	} else {
		pthread_mutex_unlock(&mtxlNiveles);
		log_error(logger, "El nivel solicitado no se encuentra conectado a la plataforma");
		sendConnectionFail(iSocketComunicacion, PL_NIVEL_INEXISTENTE, "No se encontro el nivel pedido");

		free(pHandshakePers->nombreNivel);
		free(pHandshakePers);
		return EXIT_FAILURE;
	}
}

void sendConnectionFail(int sockPersonaje, tMensaje typeMsj, char *msjInfo){
	tPaquete pkgPersonajeRepetido;
	pkgPersonajeRepetido.type   = typeMsj;
	pkgPersonajeRepetido.length = 0;
	enviarPaquete(sockPersonaje, &pkgPersonajeRepetido, logger, msjInfo);
}

bool avisoConexionANivel(int sockNivel,char *sPayload, tSimbolo simbolo){
	tMensaje tipoMensaje;
	tSimbolo simboloMsj = simbolo;
	tPaquete *paquete   = malloc(sizeof(tPaquete));
	serializarSimbolo(PL_CONEXION_PERS, simboloMsj, paquete);

	enviarPaquete(sockNivel, paquete, logger, "Envio al nivel el nuevo personaje que se conecto");
	recibirPaquete(sockNivel, &tipoMensaje, &sPayload, logger, "Recibo confirmacion del nivel");

	free(paquete);

	if (tipoMensaje == N_CONEXION_EXITOSA) {
		return true;
	} else if(tipoMensaje == N_PERSONAJE_YA_EXISTENTE) {
		return false;
	}

	return false;
}

void crearNivel(t_list* lNiveles, tNivel* pNivelNuevo, int socket, char *nombreNivel, tInfoNivel *pInfoNivel) {
	pNivelNuevo->nombre = malloc(strlen(nombreNivel) + 1);
	strcpy(pNivelNuevo->nombre, nombreNivel);
	pNivelNuevo->cListos 	 = queue_create();
	pNivelNuevo->lBloqueados = list_create();
	pNivelNuevo->socket 	 = socket;
	pNivelNuevo->quantum 	 = pInfoNivel->quantum;
	pNivelNuevo->algoritmo 	 = pInfoNivel->algoritmo;
	pNivelNuevo->delay 		 = pInfoNivel->delay;
	pNivelNuevo->maxSock 	 = socket;
	pNivelNuevo->rdDefault	 = 999;
	pthread_cond_init(&(pNivelNuevo->hayPersonajes), NULL);
	FD_ZERO(&(pNivelNuevo->masterfds));

	pthread_mutex_lock(&mtxlNiveles);
	list_add(lNiveles, pNivelNuevo);
	pthread_mutex_unlock(&mtxlNiveles);
}

void agregarPersonaje(tNivel *pNivel, tSimbolo simbolo, int socket) {
	tPersonaje *pPersonajeNuevo;
	pPersonajeNuevo = (tPersonaje *) malloc(sizeof(tPersonaje));

	pPersonajeNuevo->simbolo  	  	   = simbolo;
	pPersonajeNuevo->socket 	  	   = socket;
	pPersonajeNuevo->recursos 	  	   = list_create();
	pPersonajeNuevo->posRecurso.posX   = 0; // Solo se usa en SRDF
	pPersonajeNuevo->posRecurso.posY   = 0; // Solo se usa en SRDF
	pPersonajeNuevo->quantumUsado      = 0;
	pPersonajeNuevo->remainingDistance = pNivel->rdDefault;

	log_debug(logger, "Se agrega personaje a la cola de listos");

	queue_push(pNivel->cListos, pPersonajeNuevo);

}

void crearHiloPlanificador(pthread_t *pPlanificador, tNivel *nivelNuevo){

    if (pthread_create(pPlanificador, NULL, planificador, (void *)nivelNuevo)) {
        log_error(logger, "crearHiloPlanificador :: pthread_create: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }
    list_add(lPlanificadores, pPlanificador);
}

/*
 * PLANIFICADOR
 */
void *planificador(void * pvNivel) {

    // Armo el nivel que planifico con los parametros que recibe el hilo
    tNivel *pNivel;
    pNivel = (tNivel*) pvNivel;

    //Para multiplexar
    int iSocketConexion;
    fd_set readfds;
    FD_ZERO(&readfds);
    tMensaje tipoMensaje = NADA;
    tPersonaje* pPersonajeActual;
    char* sPayload;

    pPersonajeActual = NULL;
    int cantidadBloqueadosAntesDeDesconexion;
	int cantidadBloqueadosDespuesDeDesconexion;

    // Ciclo donde se multiplexa para escuchar personajes que se conectan
    while (1) {

    	if(tipoMensaje!=N_MUERTO_POR_ENEMIGO){
    		waitPersonajes(pNivel, &pPersonajeActual);
    	}

        iSocketConexion = multiplexar(&pNivel->masterfds, &readfds, &pNivel->maxSock, &tipoMensaje, &sPayload, logger);

        if (iSocketConexion != -1) {
            switch (tipoMensaje) {

            /* Mensajes que puede mandar el personaje */
            case(P_POS_RECURSO):
				personajeSolicitaPosicionRecurso(pNivel, pPersonajeActual, iSocketConexion, sPayload, logger);
                break;

            case(P_MOVIMIENTO):
                movimientoPersonaje(iSocketConexion, sPayload, pNivel, &pPersonajeActual);
            	break;

            case(P_SOLICITUD_RECURSO):
                solicitudRecursoPersonaje(iSocketConexion, sPayload, pNivel, &pPersonajeActual);
                break;

            //Cuando llega al recurso y aún
            case(P_FIN_TURNO):
				seleccionarJugador(&pPersonajeActual, pNivel);
            	break;

			/* Mensajes que puede mandar el nivel */
            case(N_POS_RECURSO):
				entregoPosicionRecursoAlPersonaje(pNivel, pPersonajeActual, iSocketConexion, sPayload, logger);
            	seleccionarJugador(&pPersonajeActual, pNivel);
				break;

            case(N_ACTUALIZACION_CRITERIOS):
                actualizacionCriteriosNivel(iSocketConexion, sPayload, pNivel, pPersonajeActual);
                break;

            case(N_MUERTO_POR_ENEMIGO):
				muertePorEnemigoPersonaje(pNivel, &pPersonajeActual, iSocketConexion, sPayload);
            	break;

            case(N_MUERTO_POR_DEADLOCK):
				muertePorDeadlockPersonaje(pNivel, sPayload);
				break;

            case(N_ENTREGA_RECURSO):
				recepcionRecurso(pNivel, &pPersonajeActual, sPayload);
            	break;

            case(N_BLOQUEADO_RECURSO):
				recepcionBloqueado(pNivel, &pPersonajeActual, sPayload);
            	seleccionarJugador(&pPersonajeActual, pNivel);
            	break;

            case(N_CONFIRMACION_MOV):
				confirmarMovimiento(pNivel, pPersonajeActual);
            	seleccionarJugador(&pPersonajeActual, pNivel);
				break;

            case(DESCONEXION):
            	cantidadBloqueadosAntesDeDesconexion = list_size(pNivel->lBloqueados);
				desconectar(pNivel, &pPersonajeActual, iSocketConexion);
				cantidadBloqueadosDespuesDeDesconexion = list_size(pNivel->lBloqueados);
            	if(pPersonajeActual==NULL && queue_size(pNivel->cListos)>0 && cantidadBloqueadosAntesDeDesconexion == cantidadBloqueadosDespuesDeDesconexion){
            		seleccionarJugador(&pPersonajeActual, pNivel);
            	}

				break;

            default:
            	log_error(logger, "Mensaje no esperado");
                break;

            } //Fin de switch

        } //Fin del if

    } //Fin del while

    pthread_exit(NULL);

}

int seleccionarJugador(tPersonaje** pPersonaje, tNivel* nivel) {
    int iTamanioColaListos, iTamanioListaBlock;

    // Me fijo si puede seguir jugando
    if (*pPersonaje != NULL) {

        switch(nivel->algoritmo) {
        case RR:
        	log_debug(logger, "RR: Planificando....");
            if ((*pPersonaje)->quantumUsado < nivel->quantum) {
                // Puede seguir jugando
            	enviarTurno(nivel, *pPersonaje, nivel->delay);
                return (EXIT_SUCCESS);

            } else {
                // Termina su quantum vuelve a la cola
                (*pPersonaje)->quantumUsado = 0;
            	queue_push(nivel->cListos, *pPersonaje);
            }
            break;

        case SRDF:
        	log_debug(logger, "SRDF: Planificando....");
            if ((*pPersonaje)->remainingDistance > 0) {
                // Puede seguir jugando
				enviarTurno(nivel, *pPersonaje, nivel->delay);
                return (EXIT_SUCCESS);

            } else if ((*pPersonaje)->remainingDistance == 0) {
            	//Le doy un turno para que pida el recurso
            	enviarTurno(nivel, *pPersonaje, nivel->delay);
                // Llego al recurso, se bloquea
            	return EXIT_SUCCESS;
            }
            break;
        }
    }

    // Busco al proximo personaje para darle turno
    iTamanioColaListos = queue_size(nivel->cListos);
    iTamanioListaBlock = list_size(nivel->lBloqueados);
    imprimirLista(nivel, *pPersonaje);

    if (iTamanioColaListos == 0 && iTamanioListaBlock == 0 && *pPersonaje == NULL) {
    	log_debug(logger, "No quedan mas personajes en el nivel: %s.", nivel->nombre);
    	return EXIT_SUCCESS;

    } else if (iTamanioColaListos == 1) {
    	//Si estan aqui es porque su quantum terminó
        *pPersonaje = queue_pop(nivel->cListos);
        enviarTurno(nivel, *pPersonaje, nivel->delay);

    } else if (iTamanioColaListos > 1) {
        switch(nivel->algoritmo) {
        case RR:
        	log_debug(logger, "Planificando RR...");
            *pPersonaje = queue_pop(nivel->cListos);
            (*pPersonaje)->quantumUsado = 0;
            enviarTurno(nivel, *pPersonaje, nivel->delay);
            break;

        case SRDF:
        	//Si esta aqui tiene que haber 3 o mas personajes
        	log_debug(logger, "Planificando SRDF...");

            *pPersonaje = planificacionSRDF(nivel);

            if((*pPersonaje)->remainingDistance > 0)
            	enviarTurno(nivel, *pPersonaje, nivel->delay);
            break;
        }
    }

    imprimirLista(nivel, *pPersonaje);

    return EXIT_SUCCESS;
}

void enviarTurno(tNivel *pNivel, tPersonaje *pPersonaje, int delay) {
	tPaquete pkgProximoTurno;
	pkgProximoTurno.type   = PL_OTORGA_TURNO;
	pkgProximoTurno.length = 0;
	usleep(delay);
	log_debug(logger, "%s: turno al personaje %c", pNivel->nombre, pPersonaje->simbolo);
	enviarPaquete(pPersonaje->socket, &pkgProximoTurno, logger, "Se otorga un turno al personaje");
}

bool coordinarAccesoMultiplesPersonajes(tPersonaje *personajeActual, int socketConexion, bool valor) {

	if (personajeActual != NULL) {

		if (esElPersonajeQueTieneElTurno(personajeActual->socket, socketConexion)) {
			return valor; //Devuelve el valor que corresponde

		} else {
			log_debug(logger, "Otro personaje no puede cambiar esta variable hasta que sea su turno");
			return false; //Si es otro personaje: me aseguro de no dar turnos.
		}

	} else { //Si el personaje es NULL
		log_debug(logger, "El personaje es NULL");
		return false; //Si el personaje es NULL: implica que esta en N_ENTREGA_RECURSO o en PL_SOLICITUD_RECURSO
	}
}

bool esElPersonajeQueTieneElTurno(int socketActual, int socketConexion){
	return (socketActual == socketConexion);
}

tPersonaje* planificacionSRDF(tNivel *nivel) {
    int iNroPersonaje;
    tPersonaje *pPersonaje, *pPersonajeTemp;
    int iTamanioCola = queue_size(nivel->cListos);

    //Obtengo puntero al primero
    pPersonaje = queue_peek(nivel->cListos);
    int indicePersonajeElegido = 0;

    for (iNroPersonaje = 1; iNroPersonaje < iTamanioCola; iNroPersonaje++) {
        pPersonajeTemp = list_get(nivel->cListos->elements, iNroPersonaje);
        if ((pPersonajeTemp->remainingDistance > 0) && (pPersonajeTemp->remainingDistance < pPersonaje->remainingDistance)) {
            pPersonaje = pPersonajeTemp;
            indicePersonajeElegido = iNroPersonaje;
        }
    }

    //Lo saco de listos
    pPersonaje = list_remove(nivel->cListos->elements, indicePersonajeElegido);
    return pPersonaje;
}

void entregoPosicionRecursoAlPersonaje(tNivel *pNivel, tPersonaje *pPersonajeActual, int iSocketConexion, char* sPayload, t_log* logger) {
	tPaquete pkgPosRecurso;
	obtenerDistanciaFaltante(pPersonajeActual, sPayload);

	pkgPosRecurso.type    = PL_POS_RECURSO;
	pkgPosRecurso.length  = sizeof(int8_t) + sizeof(int8_t);
	memcpy(pkgPosRecurso.payload, sPayload, pkgPosRecurso.length);
	enviarPaquete(pPersonajeActual->socket, &pkgPosRecurso, logger, "Envio de posicion de recurso al personaje");
	free(sPayload);
}

void personajeSolicitaPosicionRecurso(tNivel *pNivel, tPersonaje *pPersonajeActual, int iSocketConexion, char* sPayload, t_log* logger)
{
    tPaquete pkgPosRecurso;
    pkgPosRecurso.type   = PL_POS_RECURSO;
    pkgPosRecurso.length = sizeof(tSimbolo) + sizeof(tSimbolo);
    memcpy(pkgPosRecurso.payload, sPayload, pkgPosRecurso.length);
    free(sPayload);

    enviarPaquete(pNivel->socket, &pkgPosRecurso, logger, "Solicitud al NIVEL la posicion de recurso");
}

void obtenerDistanciaFaltante(tPersonaje *pPersonajeActual, char * sPayload) {
	tRtaPosicion *pPosicion;
	pPosicion = deserializarRtaPosicion(sPayload);

	int terminoEnX = abs(pPosicion->posX - pPersonajeActual->posRecurso.posX);
	int terminoEnY = abs(pPosicion->posY - pPersonajeActual->posRecurso.posY);

	pPersonajeActual->remainingDistance = terminoEnX + terminoEnY;

	pPersonajeActual->posRecurso.posX = pPosicion->posX;
	pPersonajeActual->posRecurso.posY = pPosicion->posY;

	free(pPosicion);
}

void muertePorEnemigoPersonaje(tNivel *pNivel, tPersonaje** pPersonajeActual, int iSocketConexion, char* sPayload) {
	tSimbolo *simbolo = deserializarSimbolo(sPayload);
	free(sPayload);

	tPersonaje *personajeMuerto;
	log_debug(logger, "<<< Me llego la muerte del personaje %c por un enemigo", *simbolo);

	//Lo saco y lo marco como muerto. Cuando se quiera volver a mover ahi lo mato
	if(*pPersonajeActual!=NULL && ((*pPersonajeActual)->simbolo==*simbolo)){ //Si el personaje que tenia el turno

		int socketPersonaje = (*pPersonajeActual)->socket;

		if(list_size((*pPersonajeActual)->recursos) > 0 ){
			char *recursosNoAsignados     = malloc(list_size((*pPersonajeActual)->recursos)*sizeof(char) + 1);
			char *personajesDesbloqueados = malloc(list_size(pNivel->lBloqueados)*sizeof(char) + 1);
			int cantidadDesbloqueados;
			int lenghtRecursos;
			liberarRecursos((*pPersonajeActual), pNivel, false, personajesDesbloqueados, &cantidadDesbloqueados, recursosNoAsignados, &lenghtRecursos);

			enviarRecursosLiberadosAlNivel(pNivel, (*pPersonajeActual)->simbolo, PL_LIBERA_RECURSOS, recursosNoAsignados, lenghtRecursos);

//			enviarPersonajesDesbloqueadosAlNivel(pNivel, (*pPersonajeActual)->simbolo, PL_LIBERA_RECURSOS, personajesDesbloqueados, cantidadDesbloqueados);

			free(recursosNoAsignados);
			free(personajesDesbloqueados);
		}

		avisarAlPersonajeDeMuerte(socketPersonaje, *simbolo, PL_MUERTO_POR_ENEMIGO);

		free((*pPersonajeActual));
		*pPersonajeActual = NULL;

	} else { //Es un personaje que no tenia el turno
		int indicePersonaje = existePersonaje(pNivel->cListos->elements, *simbolo, byName);

		if(indicePersonaje==-1){
			log_error(logger, "No se encontro el personaje que murio por enemigos");
			exit(EXIT_FAILURE);
		}
		personajeMuerto = list_remove(pNivel->cListos->elements, indicePersonaje);

		if(list_size(personajeMuerto->recursos) > 0){
			char *recursosNoAsignados     = malloc(list_size(personajeMuerto->recursos)*sizeof(char) + 1);
			char *personajesDesbloqueados = malloc(list_size(pNivel->lBloqueados)*sizeof(char)       + 1);
			int cantidadDesbloqueados;
			int lenghtRecursos;
			liberarRecursos(personajeMuerto, pNivel, false, personajesDesbloqueados, &cantidadDesbloqueados, recursosNoAsignados, &lenghtRecursos);

			//Envio recursos liberados
			enviarRecursosLiberadosAlNivel(pNivel, personajeMuerto->simbolo, PL_LIBERA_RECURSOS, recursosNoAsignados, lenghtRecursos);

//			enviarPersonajesDesbloqueadosAlNivel(pNivel, personajeMuerto->simbolo, PL_LIBERA_RECURSOS, personajesDesbloqueados, cantidadDesbloqueados);

			free(recursosNoAsignados);
			free(personajesDesbloqueados);
		}

		avisarAlPersonajeDeMuerte(personajeMuerto->socket, *simbolo, PL_MUERTO_POR_ENEMIGO);
		free(personajeMuerto);
	}

	free(simbolo);
}

int avisarAlPersonajeDeMuerte(int socketPersonajeMuerto, tSimbolo simbolo, tMensaje tipoDeMuerte){
	tPaquete pkgMuertePers;
	serializarSimbolo(tipoDeMuerte, simbolo, &pkgMuertePers);
	enviarPaquete(socketPersonajeMuerto, &pkgMuertePers, logger, "Envio mensaje de muerte por personaje");
	return EXIT_SUCCESS;
}

void muertePorDeadlockPersonaje(tNivel *pNivel, char *sPayload){

	tSimbolo *pSimboloDeadlock = deserializarSimbolo(sPayload);
	free(sPayload);

	log_debug(logger, "<<< El %s mato al personaje %c para resolver interbloqueo", pNivel->nombre, *pSimboloDeadlock);

	tPersonajeBloqueado *personajeBloqueado = getPersonajeBlock(pNivel->lBloqueados, *pSimboloDeadlock, bySymbol);
	if(personajeBloqueado != NULL){

		//Libera recursos en DESCONEXION
		avisarAlPersonajeDeMuerte(personajeBloqueado->pPersonaje->socket, *pSimboloDeadlock, PL_MUERTO_POR_DEADLOCK);
	}
	else {
		log_error(logger, "No se encontro al personaje deadlockeado en la lista de bloqueados");
		exit(EXIT_FAILURE);
	}

	free(pSimboloDeadlock);
}

void movimientoPersonaje(int iSocketConexion, char* sPayload, tNivel *pNivel, tPersonaje** pPersonajeActual) {

    tMovimientoPers *movPers = deserializarMovimientoPers(sPayload);
    free(sPayload);

	tPaquete pkgMovimientoPers;
	serializarMovimientoPers(PL_MOV_PERSONAJE, *movPers, &pkgMovimientoPers);
	enviarPaquete(pNivel->socket, &pkgMovimientoPers, logger, "Envio movimiento del personaje");

	free(movPers);
}

void confirmarMovimiento(tNivel *nivel, tPersonaje *pPersonajeActual) {
	int sockPersonaje = pPersonajeActual->socket;

	if (nivel->algoritmo == RR) {
		pPersonajeActual->quantumUsado++;
		pPersonajeActual->remainingDistance--;
	} else {
		pPersonajeActual->remainingDistance--;
		pPersonajeActual->quantumUsado = 0;
	}
	tPaquete pkgConfirmacionMov;
	pkgConfirmacionMov.type    = PL_CONFIRMACION_MOV;
	pkgConfirmacionMov.length  = 0;
	enviarPaquete(sockPersonaje, &pkgConfirmacionMov, logger, "Se envia confirmacion de movimiento al personaje");

}

int actualizacionCriteriosNivel(int iSocketConexion, char* sPayload, tNivel* pNivel, tPersonaje *pPersonajeActual) {
	tInfoNivel* pInfoNivel;
	pInfoNivel = deserializarInfoNivel(sPayload);
	free(sPayload);

	log_debug(logger, "<<< Recibe actualizacion de criterios del nivel");

	pNivel->quantum = pInfoNivel->quantum;
	pNivel->delay   = pInfoNivel->delay;

//	log_debug(logger, "POR EL INOTIFY llega            %s, %d, %d", pNivel->algoritmo, pNivel->delay, pNivel->quantum);
	if (pNivel->algoritmo != pInfoNivel->algoritmo) {
		pNivel->algoritmo = pInfoNivel->algoritmo;

		if (!nivelVacio(pNivel)) {

			if(pInfoNivel->algoritmo==SRDF){
				if (pPersonajeActual != NULL) {
					pPersonajeActual->quantumUsado = 0;
					queue_push(pNivel->cListos, pPersonajeActual);
				}

				bool _menorRemainingDistance(tPersonaje *unPersonaje, tPersonaje *otroPersonaje) {
					return (unPersonaje->remainingDistance < otroPersonaje->remainingDistance );
				}
				list_sort(pNivel->cListos->elements, (void *)_menorRemainingDistance);
			}
		}
	}

	free(pInfoNivel);
	return EXIT_SUCCESS;
}

void solicitudRecursoPersonaje(int iSocketConexion, char *sPayload, tNivel *pNivel, tPersonaje **pPersonajeActual) {
    tPaquete pkgSolicituRecurso;
    pkgSolicituRecurso.type    = PL_SOLICITUD_RECURSO;
    pkgSolicituRecurso.length  = sizeof(tSimbolo) + sizeof(tSimbolo);
    memcpy(pkgSolicituRecurso.payload, sPayload, pkgSolicituRecurso.length);

    tSimbolo *recurso;
    recurso = deserializarSimbolo(sPayload);
    free(sPayload);

    log_info(logger, "<<< El personaje %c solicita el recurso %c", (*pPersonajeActual)->simbolo, *recurso);


    free(recurso);

    enviarPaquete(pNivel->socket, &pkgSolicituRecurso, logger, "Solicitud de recurso");
}

void recepcionBloqueado(tNivel *pNivel, tPersonaje **pPersonajeActual, char *sPayload){
	tPregPosicion *recursoBloqueado;

	recursoBloqueado = deserializarPregPosicion(sPayload);
	free(sPayload);
	//Aqui actualizo su quantum
	(*pPersonajeActual)->quantumUsado 	   = pNivel->quantum;
	(*pPersonajeActual)->remainingDistance = pNivel->rdDefault;

	list_add_new((*pPersonajeActual)->recursos, (void *)&recursoBloqueado->recurso, sizeof(tSimbolo));

	tPersonajeBloqueado *pPersonajeBloqueado =  createPersonajeBlock(*pPersonajeActual, recursoBloqueado->recurso);
	list_add(pNivel->lBloqueados, pPersonajeBloqueado);
	log_debug(logger, "Personaje %c se encuentra bloqueado por recurso %c", pPersonajeBloqueado->pPersonaje->simbolo, recursoBloqueado->recurso);

	*pPersonajeActual = NULL;

}

void recepcionRecurso(tNivel *pNivel, tPersonaje **pPersonajeActual, char *sPayload) {
	tPregPosicion *recursoOtorgado;

	recursoOtorgado = deserializarPregPosicion(sPayload);
	free(sPayload);

	if (*pPersonajeActual != NULL && (*pPersonajeActual)->simbolo==recursoOtorgado->simbolo) {
	    //Aqui actualizo su quantum
		(*pPersonajeActual)->quantumUsado 	   = pNivel->quantum;
		(*pPersonajeActual)->remainingDistance = pNivel->rdDefault;

		list_add_new((*pPersonajeActual)->recursos, (void *)&recursoOtorgado->recurso, sizeof(tSimbolo));

		tPaquete pkgRecursoOtorgado;
		pkgRecursoOtorgado.type   = PL_RECURSO_OTORGADO;
		pkgRecursoOtorgado.length = 0;
		enviarPaquete((*pPersonajeActual)->socket, &pkgRecursoOtorgado, logger, "Se confirma otorgamiento de recurso al personaje");

		queue_push(pNivel->cListos, *pPersonajeActual);
		*pPersonajeActual=NULL;

	} else {

		log_warning(logger, "El personaje que recibio el recurso no era el actual. Lo busco en la lista");
		sleep(2);
		tPersonaje *pPersonaje = getPersonaje(pNivel->cListos->elements, recursoOtorgado->simbolo, byName);
		 //Aqui actualizo su quantum
		pPersonaje->quantumUsado 	   = pNivel->quantum;
		pPersonaje->remainingDistance = pNivel->rdDefault;

		list_add_new(pPersonaje->recursos, (void *)&recursoOtorgado->recurso, sizeof(tSimbolo));

		tPaquete pkgRecursoOtorgado;
		pkgRecursoOtorgado.type   = PL_RECURSO_OTORGADO;
		pkgRecursoOtorgado.length = 0;
		enviarPaquete(pPersonaje->socket, &pkgRecursoOtorgado, logger, "Se confirma otorgamiento de recurso al personaje");

	}

	free(recursoOtorgado);
}

int desconectar(tNivel *pNivel, tPersonaje **pPersonajeActual, int iSocketConexion) {

	log_info(logger, "<<< Se detecta desconexion...");

	if (iSocketConexion == pNivel->socket) {
		desconectarNivel(pNivel);
	} else {
		desconectarPersonaje(pNivel, pPersonajeActual, iSocketConexion);
	}

	return EXIT_SUCCESS;
}

int desconectarNivel(tNivel *pNivel){
	log_error(logger, "Se desconecto el %s", pNivel->nombre);
	pthread_mutex_lock(&mtxlNiveles);
	int indiceNivel = existeNivel(listaNiveles, pNivel->nombre);
	if(indiceNivel == -1){
		log_error(logger, "No se encontro el nivel desconectado");
		exit(EXIT_FAILURE);
	}

	pNivel = list_remove(listaNiveles, indiceNivel);
	destroyNivel(pNivel);
	pthread_mutex_unlock(&mtxlNiveles);
	pthread_t *pPlanificador = list_remove(lPlanificadores, indiceNivel);
	destroyPlanificador(pPlanificador);
	return EXIT_FAILURE;
}

int desconectarPersonaje(tNivel *pNivel, tPersonaje **pPersonajeActual, int iSocketConexion){
	tPersonaje *pPersonaje;
	int socketPersonajeQueSalio;
	bool bloqueado=false;

	if ((*pPersonajeActual)!= NULL && (iSocketConexion == (*pPersonajeActual)->socket)) {
		socketPersonajeQueSalio = iSocketConexion;
//		pthread_mutex_lock(&mtxlNiveles);
//		delegarConexion(&setSocketsOrquestador, &pNivel->masterfds, socketPersonajeQueSalio, &iSocketMaximoOrquestador);
//		pthread_mutex_unlock(&mtxlNiveles);
		liberarRecursosYDesbloquearPersonajes(pNivel, *pPersonajeActual, bloqueado);
		*pPersonajeActual = NULL;
		return socketPersonajeQueSalio;
	}


	pPersonaje = sacarPersonajeDeListas(pNivel, iSocketConexion, &bloqueado);
	if (pPersonaje != NULL) {
		socketPersonajeQueSalio = pPersonaje->socket;
//		pthread_mutex_lock(&mtxlNiveles);
//		delegarConexion(&setSocketsOrquestador, &pNivel->masterfds, socketPersonajeQueSalio, &iSocketMaximoOrquestador);
//		pthread_mutex_unlock(&mtxlNiveles);
		liberarRecursosYDesbloquearPersonajes(pNivel, pPersonaje, bloqueado);
		return socketPersonajeQueSalio;
	}
	else {
		log_error(logger, "No se encontró el personaje que salio en socket %d", iSocketConexion);
		return iSocketConexion;
	}
}

void enviarRecursosLiberadosAlNivel(tNivel *pNivel, tSimbolo simboloPersonaje, tMensaje tipoMensaje, char *recursosLiberados, int cantidad){
	tPaquete pkgDesconexion;
	tDesconexionPers desconexionPersonaje;

	desconexionPersonaje.simbolo = simboloPersonaje;
	desconexionPersonaje.lenghtRecursos = cantidad;
	if(cantidad!=0)
		memcpy(&desconexionPersonaje.recursos, recursosLiberados, desconexionPersonaje.lenghtRecursos);
	serializarDesconexionPers(tipoMensaje, desconexionPersonaje, &pkgDesconexion);

	enviarPaquete(pNivel->socket, &pkgDesconexion, logger, "Se envia desconexion del personaje al nivel");

}

void enviarPersonajesDesbloqueadosAlNivel(tNivel *pNivel, tSimbolo simboloPersonaje, tMensaje tipoMensaje, char *personajesDesbloqueados, int cantidad){
	enviarRecursosLiberadosAlNivel(pNivel, simboloPersonaje, tipoMensaje, personajesDesbloqueados, cantidad);
}

int liberarRecursosYDesbloquearPersonajes(tNivel *pNivel, tPersonaje *pPersonajeDesconectado, bool bloqueado){
	log_info(logger, "Se desconecto el personaje %c", pPersonajeDesconectado->simbolo);
	int lenghtRecursos;
	char *recursosNoAsignados     = malloc(list_size(pPersonajeDesconectado->recursos)*sizeof(char) + 1);
	char *personajesDesbloqueados = malloc(list_size(pNivel->lBloqueados)*sizeof(char)              + 1);
	int cantidadDesbloqueados;
	liberarRecursos(pPersonajeDesconectado, pNivel, bloqueado, personajesDesbloqueados, &cantidadDesbloqueados, recursosNoAsignados, &lenghtRecursos);

	//Envio recursos liberados
	enviarRecursosLiberadosAlNivel(pNivel, pPersonajeDesconectado->simbolo, PL_DESCONEXION_PERSONAJE, recursosNoAsignados, lenghtRecursos);

	enviarPersonajesDesbloqueadosAlNivel(pNivel, pPersonajeDesconectado->simbolo, PL_DESCONEXION_PERSONAJE, personajesDesbloqueados, cantidadDesbloqueados);

	free(pPersonajeDesconectado);
	free(personajesDesbloqueados);
	free(recursosNoAsignados);
	return EXIT_SUCCESS;
}

//BOrrrar
//void avisarDesconexionAlNivel(tNivel *pNivel, tPersonaje *pPersonaje, char * personajesDesbloqueados, int cantidadDesbloqueados, int lenghtRecursos, char **recursosLiberados){
//	tPaquete pkgDesconexionPers;
//	tDesconexionPers desconexionPersonaje;
//
//	desconexionPersonaje.simbolo = pPersonaje->simbolo;
//	desconexionPersonaje.lenghtRecursos = lenghtRecursos;
//	if(lenghtRecursos==0)
//		*recursosLiberados = NULL;
//	else
//		memcpy(&desconexionPersonaje.recursos, *recursosLiberados, lenghtRecursos);
//	serializarDesconexionPers(PL_DESCONEXION_PERSONAJE, desconexionPersonaje, &pkgDesconexionPers);
//
//	enviarPaquete(pNivel->socket, &pkgDesconexionPers, logger, "Se envia desconexion del personaje al nivel");
//}

/*
 * Se liberan los recursos que poseia el personaje en el nivel y en caso de que un personaje estaba bloqueado por uno de estos, se libera
 */
void liberarRecursos(tPersonaje *pPersMuerto, tNivel *pNivel, bool bloqueado, char *personajesDeadloqueados, int *cantDesbloqueados, char *recursosNoAsignados, int *lengthRecursos) {
	int iCantidadBloqueados = list_size(pNivel->lBloqueados);
	int iCantidadRecursos	= list_size(pPersMuerto->recursos);

	if ((iCantidadBloqueados == 0) || (iCantidadRecursos == 0)) {
		/* No hay nada que liberar */
		log_info(logger, "No se reasignarán recursos con la muerte de %c", pPersMuerto->simbolo);
		*lengthRecursos = iCantidadRecursos;
		getRecursosNoAsignados(pPersMuerto->recursos, recursosNoAsignados, lengthRecursos, bloqueado);
		*cantDesbloqueados = 0;
		personajesDeadloqueados = NULL;
	} else {

		int iIndexBloqueados, iIndexRecursos;
		tPersonaje *pPersonajeLiberado;
		tSimbolo *pRecurso;
		*cantDesbloqueados = 0;

		/* Respetando el orden en que se bloquearon voy viendo si se libero el recurso que esperaban */
		for (iIndexBloqueados = 0; iIndexBloqueados < list_size(pNivel->lBloqueados); iIndexBloqueados++) {
			tPersonajeBloqueado *pPersonajeBloqueado = (tPersonajeBloqueado *)list_get(pNivel->lBloqueados, iIndexBloqueados);

			//Si el personaje que se murio estaba bloquedao entonces el ultimo recuros de su lista no es real.
			for (iIndexRecursos = 0; iIndexRecursos < (list_size(pPersMuerto->recursos) - (bloqueado ? 1 : 0)); iIndexRecursos++) {
				pRecurso = (tSimbolo *)list_get(pPersMuerto->recursos, iIndexRecursos);

				if (pPersonajeBloqueado->recursoEsperado == *pRecurso) {
					pPersonajeBloqueado = (tPersonajeBloqueado *)list_remove(pNivel->lBloqueados, iIndexBloqueados);
					personajesDeadloqueados[*cantDesbloqueados] = pPersonajeBloqueado->pPersonaje->simbolo;
					*cantDesbloqueados = *cantDesbloqueados + 1;

					/* Como saco un personaje de la lista, actualizo la iteracion de los bloqueados */
					pPersonajeLiberado = pPersonajeBloqueado->pPersonaje;

					pRecurso = (tSimbolo *)list_remove(pPersMuerto->recursos, iIndexRecursos);
					/* Como saco un recurso de la lista, actualizo la iteracion de los recursos */

					log_info(logger, "Por la muerte de %c se desbloquea %c que estaba esperando por el recurso %c", pPersMuerto->simbolo, pPersonajeLiberado->simbolo, *pRecurso);

					queue_push(pNivel->cListos, pPersonajeLiberado);

					tPaquete paquete;
					paquete.length = 0;
					paquete.type   = PL_RECURSO_OTORGADO;
					enviarPaquete(pPersonajeLiberado->socket, &paquete, logger, "Se le otorgo el recurso al personaje");

					free(pPersonajeBloqueado);
					iIndexBloqueados--;
					iIndexRecursos--;
					break;
				}
			}
		}

		getRecursosNoAsignados(pPersMuerto->recursos, recursosNoAsignados, lengthRecursos, bloqueado);
	}
}

/*
 * Recibe una lista de recursos de un personaje muerto y retorna un puntero a string con esos recursos; y libera memoria de la lista
 */
char *getRecursosNoAsignados(t_list *recursos, char *recursosNoAsignados, int *lengthRecursos, bool bloqueado){

	int cantRecursosNoAsignados = *lengthRecursos = (list_size(recursos) - (bloqueado ? 1 : 0));
	int i;
	tSimbolo *pRecurso;
	if(cantRecursosNoAsignados != 0){

		for(i = 0; i < cantRecursosNoAsignados - (bloqueado ? 1 : 0); i++){
			pRecurso = (tSimbolo*) list_get(recursos, i);
			recursosNoAsignados[i] = (char)*pRecurso;
		}
		list_destroy_and_destroy_elements(recursos, free);
		return recursosNoAsignados;
	}
	return NULL;
}

/*
 * Verificar si el nivel existe, si existe devuelve el indice de su posicion, sino devuelve -1
 */
int existeNivel(t_list * lNiveles, char* sLevelName) {
	int iNivelLoop;
	int iCantNiveles = list_size(lNiveles);
	tNivel* pNivelGuardado;

	if (iCantNiveles > 0) {

		for (iNivelLoop = 0; iNivelLoop < iCantNiveles; iNivelLoop++) {
			pNivelGuardado = (tNivel *)list_get(listaNiveles, iNivelLoop);

			if (string_equals_ignore_case(pNivelGuardado->nombre, sLevelName)) {
				pthread_mutex_unlock(&mtxlNiveles);
				return iNivelLoop;
			}
		}
	}
	return -1;
}

tPersonaje *getPersonaje(t_list *listaPersonajes, int valor, tBusquedaPersonaje criterio){
	int iPersonajeLoop;
	int iCantPersonajes = list_size(listaPersonajes);
	tPersonaje* pPersonajeGuardado;

	if (iCantPersonajes > 0) {

		for (iPersonajeLoop = 0; iPersonajeLoop < iCantPersonajes; iPersonajeLoop++) {
			pPersonajeGuardado = (tPersonaje *)list_get(listaPersonajes, iPersonajeLoop);
			switch(criterio) {
			case bySocket:
				if (pPersonajeGuardado->socket == valor) {
					return pPersonajeGuardado;
				}
				break;

			case byName:
				if (pPersonajeGuardado->simbolo == (tSimbolo)valor) {
					return pPersonajeGuardado;
				}
				break;
			}
		}
	}
	return NULL;
}

tPersonajeBloqueado *getPersonajeBlock(t_list *lBloqueados, int valor, tBusquedaPersBlock criterio) {
	int iPersonajeLoop;
	int iCantPersonajes = list_size(lBloqueados);
	tPersonajeBloqueado* pPersonajeGuardado;

	if (iCantPersonajes > 0) {

		for (iPersonajeLoop = 0; iPersonajeLoop < iCantPersonajes; iPersonajeLoop++) {
			pPersonajeGuardado = (tPersonajeBloqueado *)list_get(lBloqueados, iPersonajeLoop);

			switch(criterio) {
			case bySock:
				if (pPersonajeGuardado->pPersonaje->socket == valor) {
					return pPersonajeGuardado;
				}
				break;

			case bySymbol:
				if (pPersonajeGuardado->pPersonaje->simbolo == (tSimbolo)valor) {
					return pPersonajeGuardado;
				}
				break;

			case byRecursoBlock:
				if (pPersonajeGuardado->recursoEsperado == (tSimbolo)valor) {
					return pPersonajeGuardado;
				}
				break;
			}
		}
	}
	return NULL;
}

/*
 * Verificar si el personaje existe en base a un criterio, si existe devuelve el indice de su posicion, sino devuelve -1
 */
int existePersonaje(t_list *pListaPersonajes, int valor, tBusquedaPersonaje criterio) {
	int iPersonajeLoop;
	int iCantPersonajes = list_size(pListaPersonajes);
	tPersonaje* pPersonajeGuardado;

	if (iCantPersonajes > 0) {
		for (iPersonajeLoop = 0; iPersonajeLoop < iCantPersonajes; iPersonajeLoop++) {
			pPersonajeGuardado = (tPersonaje *)list_get(pListaPersonajes, iPersonajeLoop);
			switch(criterio){
			case bySocket:
				if(pPersonajeGuardado->socket == valor)
					return iPersonajeLoop;
				break;
			case byName:
				if(pPersonajeGuardado->simbolo == (tSimbolo)valor)
					return iPersonajeLoop;
				break;
			}
		}
	}
	return -1;
}

/*
 * Verificar si el personaje existe en bloqueados, si existe devuelve el indice de su posicion, sino devuelve -1
 */
int existPersonajeBlock(t_list *block, tSimbolo valor, tBusquedaPersBlock criterio){
	int i;
	int iCantBlock = list_size(block);
	tPersonajeBloqueado *pPersonajeGuardado;

	for (i = 0; (i < iCantBlock); i++) {
		pPersonajeGuardado = (tPersonajeBloqueado *)list_get(block, i);
		switch(criterio) {

		case bySock:
			if (pPersonajeGuardado->pPersonaje->socket == valor) {
				return i;
			}
			break;

		case bySymbol:
			if (pPersonajeGuardado->pPersonaje->simbolo == (tSimbolo)valor) {
				return i;
			}
			break;

		case byRecursoBlock:
			if (pPersonajeGuardado->recursoEsperado == (tSimbolo)valor) {
				return i;
			}
			break;
		}
	}
	return -1;
}

/*
 * Elimina al personaje que contiene el socket pasado por parametro de todas las listas del nivel y lo devuelve, si no lo encuentra devuelve null
 */
tPersonaje *sacarPersonajeDeListas(tNivel *pNivel, int iSocket, bool *bloqueado) {
	int iIndicePersonaje;

	iIndicePersonaje = existePersonaje(pNivel->cListos->elements, iSocket, bySocket);

	if (iIndicePersonaje != -1) {
		tPersonaje *pPersonaje =  list_remove(pNivel->cListos->elements, iIndicePersonaje);
		*bloqueado = false;
		return pPersonaje;
	}

	iIndicePersonaje = existPersonajeBlock(pNivel->lBloqueados, iSocket, bySock);

	if (iIndicePersonaje != -1) {
		*bloqueado = true;
		tPersonajeBloqueado *pPersonajeBlock = list_remove(pNivel->lBloqueados, iIndicePersonaje);
		return pPersonajeBlock->pPersonaje;
	}

	return NULL;
}

void imprimirLista(tNivel *pNivel, tPersonaje *pPersonaje) {

	char* tmp = malloc(30);
	char* retorno = malloc(500);
	int i;
	tPersonaje *pPersAux;
	tPersonajeBloqueado * pPersonajeBlock;

	if (queue_is_empty(pNivel->cListos)) {
		if(pPersonaje == NULL){
			sprintf(retorno, "Lista de: %s\n\tEjecutando:\n\tListos: \t", pNivel->nombre);
		} else {
			sprintf(retorno, "Lista de: %s\n\tEjecutando: %c\n\tListos: \t", pNivel->nombre, pPersonaje->simbolo);
		}
	} else {
		if (pPersonaje != NULL) {
			sprintf(retorno, "Lista de: %s\n\tEjecutando: %c\n\tListos: \t", pNivel->nombre, pPersonaje->simbolo);
		} else {
			sprintf(retorno, "Lista de: %s\n\tEjecutando:\n\tListos: \t", pNivel->nombre);
		}
	}

	int iCantidadListos = queue_size(pNivel->cListos);
	for (i = 0; i < iCantidadListos; i++) {
		pPersAux = (tPersonaje *)list_get(pNivel->cListos->elements, i);
		if(pPersonaje != NULL){ //Si no es NULL fijate que no sea el mismo que esta jugando
			if((pPersonaje->simbolo != pPersAux->simbolo)){
				sprintf(tmp, "%c -> ", pPersAux->simbolo);
				string_append(&retorno, tmp);
			}
		} else { //Si es NULL segui appendeando como siempre
			sprintf(tmp, "%c -> ", pPersAux->simbolo);
			string_append(&retorno, tmp);
		}
	}

	sprintf(tmp, "\n\tBloqueados: \t");
	string_append(&retorno, tmp);

	int iCantidadBloqueados = list_size(pNivel->lBloqueados);
	for (i = 0; i < iCantidadBloqueados; i++) {
		pPersonajeBlock = (tPersonajeBloqueado *) list_get(pNivel->lBloqueados, i);
		if(pPersonaje != NULL){ //Si no es NULL fijate que no sea el mismo que esta jugando
			if((pPersonaje->simbolo != pPersonajeBlock->pPersonaje->simbolo)){
				sprintf(tmp, "%c -> ", pPersonajeBlock->pPersonaje->simbolo);
				string_append(&retorno, tmp);
			}
		} else { //Si es NULL segui appendeando como siempre
			sprintf(tmp, "%c -> ", pPersonajeBlock->pPersonaje->simbolo);
			string_append(&retorno, tmp);
		}
	}

	log_info(logger, retorno);

	free(tmp);
	free(retorno);

}

tPersonajeBloqueado* sacarDeListaBloqueados(t_list* lBloqueados, tSimbolo simbolo) {
	tPersonajeBloqueado *pPersonajeBloqueado;
	int iCantidadBloqueados;
	int iIndicePersonaje;
	int bEncontrado = 0;

	iCantidadBloqueados = list_size(lBloqueados);

	for (iIndicePersonaje = 0; (iIndicePersonaje < iCantidadBloqueados) && (bEncontrado == 0); iIndicePersonaje++) {
		pPersonajeBloqueado = (tPersonajeBloqueado *)list_get(lBloqueados, iIndicePersonaje);
		bEncontrado    		= (pPersonajeBloqueado->pPersonaje->simbolo == simbolo);
	}

	if (bEncontrado == 1) {
		pPersonajeBloqueado = list_remove(lBloqueados, iIndicePersonaje);
		return pPersonajeBloqueado;

	} else {
		return NULL;
	}
}

/*
 * Devuelve un puntero al primer personaje que se encontraba esperando el recurso en la lista de bloqueados
 */
tPersonaje* desbloquearPersonaje(t_list* lBloqueados, tSimbolo simboloPersonaje) {
	int iIndicePersonaje;

	iIndicePersonaje = existPersonajeBlock(lBloqueados, simboloPersonaje, bySymbol);

	if (iIndicePersonaje != -1) {
		tSimbolo recurso;
		tPersonaje *personaje = removePersonajeOfBlock(lBloqueados, &recurso, iIndicePersonaje);

		if (personaje == NULL) {
			log_debug(logger, "personaje es NULL");
		}
		list_add_new(personaje->recursos, (void *)&recurso, sizeof(recurso));
		return personaje;

	} else {
		return NULL;
	}
}

void delegarConexion(fd_set *conjuntoDestino, fd_set *conjuntoOrigen, int iSocket, int *maxSock) {

	FD_CLR(iSocket, conjuntoOrigen); // Saco el socket del conjunto de sockets del origen
	FD_SET(iSocket, conjuntoDestino); //Lo agrego al conjunto destino

	if (FD_ISSET(iSocket, conjuntoDestino)) {
		log_debug(logger, "--> Se delega la conexion <--");

	} else {
		log_error(logger, "Error al delegar conexiones");
		exit(EXIT_FAILURE);
	}

	//Actualizo el tope del set de sockets
	if (iSocket > *maxSock) {
		*maxSock = iSocket;
	}
}

void inicializarConexion(fd_set *master_planif, int *maxSock, int *sock) {
	FD_ZERO(master_planif);
	*maxSock = *sock;
}

void waitPersonajes(tNivel *pNivel, tPersonaje **pPersonajeActual) {
	if (nivelVacio(pNivel) && *pPersonajeActual==NULL) {
		log_debug(logger, "Wait");
		pthread_mutex_lock(&semNivel);
		pthread_cond_wait(&pNivel->hayPersonajes, &semNivel);
		pthread_mutex_unlock(&semNivel);
		//Le doy el turno al primer jugador
		seleccionarJugador(pPersonajeActual, pNivel);
	}
}

/*
 * Crea estructura de personaje bloqueado y alocando memoria y returna un puntero al personaje creado
 */
tPersonajeBloqueado *createPersonajeBlock(tPersonaje *personaje, tSimbolo recurso){
	 tPersonajeBloqueado *pPersonajeBloqueado = malloc(sizeof(tPersonajeBloqueado));
	 pPersonajeBloqueado->pPersonaje = personaje;
	 memcpy(&pPersonajeBloqueado->recursoEsperado, &recurso, sizeof(tSimbolo));
	 return pPersonajeBloqueado;
}

/*
 * Retorna un puntero a un personaje de la lista de bloqueados y libera memoria
 */
tPersonaje *removePersonajeOfBlock(t_list *block, tSimbolo *recurso, int indicePersonaje){
	tPersonaje *personaje;
	tPersonajeBloqueado *personajeBlock;
	personajeBlock = list_remove(block, indicePersonaje);
	personaje = personajeBlock->pPersonaje;
	*recurso = personajeBlock->recursoEsperado;
	free(personajeBlock);
	return personaje;
}

