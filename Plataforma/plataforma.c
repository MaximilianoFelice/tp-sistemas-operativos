/*
 * Sistemas Operativos - Super Mario Proc RELOADED.
 * Grupo       : C o no ser.
 * Nombre      : plataforma.c.
 * Descripcion : Este archivo contiene la implementacion de las
 * funciones usadas por la plataforma.
 */

#include "plataforma.h"

pthread_mutex_t semNivel;
pthread_cond_t hayPersonajes;
t_list 	 *listaNiveles;
t_config *configPlataforma;
t_log *logger;
unsigned short usPuerto;
bool hay_personajes;


/*
 * Funciones privadas
 */
tPersonaje* desbloquearPersonaje(t_list* lBloqueados, tSimbolo recurso);
int existePersonaje(t_list *pListaPersonajes, int iSocket);
tPersonaje *sacarPersonajeDeListas(tNivel *pNivel, int iSocket);


/*
 * Funciones privadas plataforma
 */
int executeKoopa(char *koopaPath, char *scriptPath);


/*
 * Funciones privadas orquestador
 */
int conexionNivel(int iSocketComunicacion, char* sPayload, fd_set* pSetSocketsOrquestador, t_list *lPlanificadores);
int conexionPersonaje(int iSocketComunicacion, fd_set* socketsOrquestador, char* sPayload);
bool avisoConexionANivel(int sockNivel,char *sPayload, tSimbolo simbolo);
void sendPersonajeRepetido(int socketPersonaje);
void sendNivelRepetido(int sockPersonaje);
void crearHiloPlanificador(pthread_t *pPlanificador, tNivel *nivelNuevo, t_list *lPlanificadores);


/*
 * Funciones privadas planificador
 */
int seleccionarJugador(tPersonaje** pPersonaje, tNivel* nivel, int iEnviarTurno);
tPersonaje* planificacionSRDF(t_queue* cListos, int iTamanioCola);
void solicitudRecursoPersonaje(int iSocketConexion, char *sPayload, tNivel *pNivel, tPersonaje **pPersonajeActual, t_log *logger);
void movimientoPersonaje(int iSocketConexion, char* sPayload, tNivel *pNivel, tPersonaje* pPersonaje, t_log* logger);
void posicionRecursoPersonaje(int iSocketConexion, char* sPayload, int socketNivel, t_log* logger);
int actualizacionCriteriosNivel(int iSocketConexion, char* sPayload, tNivel* pNivel, t_log* logger);
void muertePorEnemigoPersonaje(char* sPayload, tNivel *pNivel, tPersonaje* pPersonaje, t_log* logger);
void recepcionRecurso(tNivel *pNivel, char *sPayload, t_log *logger);
void enviarTurno(tPersonaje *pPersonaje);
void confirmarMovimiento(tPersonaje *pPersonajeActual, t_log *logger);
void desconectar(tNivel *pNivel, tPersonaje **pPersonajeActual, int iSocketConexion, char *sPayload);

/*
 * PLATAFORMA
 */

int main(int argc, char*argv[]) {

	if (argc <= 1) {
		printf("[ERROR] Se debe pasar como parametro el archivo de configuracion.\n");
		exit(EXIT_FAILURE);
	}

	char *pathKoopa;
	char *pathScript;
	int thResult;
	pthread_t thOrquestador;
	hay_personajes = false;

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
	pthread_mutex_init(&semNivel, NULL );
	pthread_cond_init(&hayPersonajes, NULL);

	thResult = pthread_create(&thOrquestador, NULL, orquestador, (void *) &usPuerto);

	if (thResult != 0) {
		log_error(logger, "No se pudo crear el hilo orquestador.");
		exit(EXIT_FAILURE);
	}

	pthread_join(thOrquestador, NULL);
	log_destroy(logger);

	executeKoopa(pathKoopa, pathScript);

	exit(EXIT_SUCCESS);
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

/*
 * ORQUESTADOR
 */

void *orquestador(void *vPuerto) {
	t_list *lPlanificadores;

	unsigned short usPuerto;
	usPuerto = *(unsigned short*)vPuerto;

	// Creo la lista de niveles y planificadores
	lPlanificadores = list_create();
	listaNiveles    = list_create();

	// Definicion de variables para los sockets
	fd_set setSocketsOrquestador;
	FD_ZERO(&setSocketsOrquestador);
	int iSocketEscucha, iSocketComunicacion, iSocketMaximo;

	// Inicializacion de sockets
	iSocketEscucha = crearSocketEscucha(usPuerto, logger);
	FD_SET(iSocketEscucha, &setSocketsOrquestador);
	iSocketMaximo = iSocketEscucha;

	tMensaje tipoMensaje;
	char * sPayload;

	while (1) {
		iSocketComunicacion = getConnection(&setSocketsOrquestador, &iSocketMaximo, iSocketEscucha, &tipoMensaje, &sPayload, logger);

		if (iSocketComunicacion != -1) {

			pthread_mutex_lock(&semNivel);
	
			switch (tipoMensaje) {
			case N_HANDSHAKE: // Un nuevo nivel se conecta
				conexionNivel(iSocketComunicacion, sPayload, &setSocketsOrquestador, lPlanificadores);
				break;

			case P_HANDSHAKE:
				conexionPersonaje(iSocketComunicacion, &setSocketsOrquestador, sPayload);
				break;

			default:
				break;
			}

			pthread_mutex_unlock(&semNivel);

		}

	}

	pthread_exit(NULL);
}

int conexionNivel(int iSocketComunicacion, char* sPayload, fd_set* pSetSocketsOrquestador, t_list *lPlanificadores) {

	int iIndiceNivel;
	iIndiceNivel = existeNivel(listaNiveles, sPayload);

	if (iIndiceNivel >= 0) {
		tPaquete pkgNivelRepetido;
		pkgNivelRepetido.type   = PL_NIVEL_YA_EXISTENTE;
		pkgNivelRepetido.length = 0;
		enviarPaquete(iSocketComunicacion, &pkgNivelRepetido, logger, "Ya se encuentra conectado al orquestador un nivel con el mismo nombre");
		return EXIT_FAILURE;
	}

	pthread_t *pPlanificador;
	tMensaje tipoMensaje;
	tInfoNivel *pInfoNivel;

	tNivel *pNivelNuevo = (tNivel *) malloc(sizeof(tNivel));
	pPlanificador 	   = (pthread_t *) malloc(sizeof(pthread_t));
	log_debug(logger, "Se conecto el nivel %s", sPayload);
	char* sNombreNivel = strdup(sPayload);

	tPaquete pkgHandshake;
	pkgHandshake.type   = PL_HANDSHAKE;
	pkgHandshake.length = 0;
	enviarPaquete(iSocketComunicacion, &pkgHandshake, logger, "Handshake Plataforma");

	// Ahora debo esperar a que me llegue la informacion de planificacion.
	recibirPaquete(iSocketComunicacion, &tipoMensaje, &sPayload, logger, "Recibe mensaje informacion del nivel");

	pInfoNivel = deserializarInfoNivel(sPayload);

	// Validacion de que el nivel me envia informacion correcta
	if (tipoMensaje == N_DATOS) {

		crearNivel(listaNiveles, pNivelNuevo, iSocketComunicacion, sNombreNivel, pInfoNivel);
		crearHiloPlanificador(pPlanificador, pNivelNuevo, lPlanificadores);
		delegarConexion(&pNivelNuevo->masterfds, pSetSocketsOrquestador, iSocketComunicacion, &pNivelNuevo->maxSock);

		// Logueo el nuevo hilo recien creado
		log_debug(logger, "Nuevo planificador del nivel: '%s' y planifica con: %i", pNivelNuevo->nombre, pNivelNuevo->algoritmo);

	} else {
		log_error(logger,"Tipo de mensaje incorrecto: se esperaba datos del nivel");
		return EXIT_FAILURE;
	}

	free(sPayload);
	free(pInfoNivel);
	return EXIT_SUCCESS;
}

int conexionPersonaje(int iSocketComunicacion, fd_set* socketsOrquestador, char* sPayload) {
	tHandshakePers* pHandshakePers;
	pHandshakePers = deserializarHandshakePers(sPayload);
	int iIndiceNivel;

	log_info(logger, "Se conectó el personaje %c pidiendo el nivel '%s'", pHandshakePers->simbolo, pHandshakePers->nombreNivel);

	iIndiceNivel = existeNivel(listaNiveles, pHandshakePers->nombreNivel);

	if (iIndiceNivel >= 0) {
		tNivel *pNivelPedido;
		pNivelPedido = list_get_data(listaNiveles, iIndiceNivel);
		if(pNivelPedido == NULL)
			log_error(logger, "Saco mal el nivel: Puntero en NULL");

		bool rta_nivel = avisoConexionANivel(pNivelPedido->socket, sPayload, pHandshakePers->simbolo);

		if(rta_nivel){
			agregarPersonaje(pNivelPedido->cListos, pHandshakePers->simbolo, iSocketComunicacion);
			delegarConexion(&pNivelPedido->masterfds, socketsOrquestador, iSocketComunicacion, &pNivelPedido->maxSock);

			signal_personajes();

			tPaquete pkgHandshake;
			pkgHandshake.type   = PL_HANDSHAKE;
			pkgHandshake.length = 0;
			// Le contesto el handshake
			enviarPaquete(iSocketComunicacion, &pkgHandshake, logger, "Handshake de la plataforma al personaje");
		} else {
			sendPersonajeRepetido(iSocketComunicacion); //TODO que el personaje maneje este mensaje
			return EXIT_FAILURE;
		}

	} else {
		sendNivelRepetido(iSocketComunicacion);
		return EXIT_FAILURE;
	}

	free(sPayload);
	free(pHandshakePers);
	return EXIT_SUCCESS;
}

void sendNivelRepetido(int sockPersonaje){
	log_error(logger, "El nivel solicitado no se encuentra conectado a la plataforma");
	tPaquete pkgNivelInexistente;
	pkgNivelInexistente.type   = PL_NIVEL_INEXISTENTE;
	pkgNivelInexistente.length = 0;
	enviarPaquete(sockPersonaje, &pkgNivelInexistente, logger, "No se encontro el nivel pedido");
}

void sendPersonajeRepetido(int socketPersonaje){
	log_error(logger, "El personaje ya esta en ese nivel"); //TODO que el personaje trate este mensaje
	tPaquete pkgPersonajeRepetido;
	pkgPersonajeRepetido.type   = PL_PERSONAJE_REPETIDO;
	pkgPersonajeRepetido.length = 0;
	enviarPaquete(socketPersonaje, &pkgPersonajeRepetido, logger, "El personaje ya esta jugando ese nivel");
}

bool avisoConexionANivel(int sockNivel,char *sPayload, tSimbolo simbolo){

	tMensaje tipoMensaje;
	tSimbolo simboloMsj = simbolo;
	tPaquete *paquete = malloc(sizeof(tPaquete));
	serializarSimbolo(PL_CONEXION_PERS, simboloMsj, paquete);

	enviarPaquete(sockNivel, paquete, logger, "Envio al nivel el nuevo personaje que se conecto");
	recibirPaquete(sockNivel, &tipoMensaje, &sPayload, logger, "Recibo confirmacion del nivel");

	free(paquete);

	if(tipoMensaje == N_CONEXION_EXITOSA){
		return true;
	} else if(tipoMensaje == N_PERSONAJE_YA_EXISTENTE){
		return false;
	}
	return false;
}

void crearNivel(t_list* lNiveles, tNivel* pNivelNuevo, int socket, char *levelName, tInfoNivel *pInfoNivel) {
	pNivelNuevo->nombre = malloc(strlen(levelName) + 1);
	strcpy(pNivelNuevo->nombre, levelName);
	pNivelNuevo->cListos 	 = queue_create();
	pNivelNuevo->lBloqueados = list_create();
	pNivelNuevo->lMuertos 	 = list_create();
	pNivelNuevo->socket 	 = socket;
	pNivelNuevo->quantum 	 = pInfoNivel->quantum;
	pNivelNuevo->algoritmo 	 = pInfoNivel->algoritmo;
	pNivelNuevo->delay 		 = pInfoNivel->delay;
	pNivelNuevo->maxSock 	 = socket;
	//pNivelNuevo->hay_personajes = false;
	FD_ZERO(&(pNivelNuevo->masterfds));

	list_add(lNiveles, pNivelNuevo);
}

void agregarPersonaje(t_queue *cPersonajes, tSimbolo simbolo, int socket) {

	log_debug(logger, "Apunto de agregar a un personaje");
	tPersonaje *pPersonajeNuevo;
	pPersonajeNuevo = malloc(sizeof(tPersonaje));

	pPersonajeNuevo->simbolo  	  	= simbolo;
	pPersonajeNuevo->socket 	  	= socket;
	pPersonajeNuevo->recursos 	  	= list_create();
	pPersonajeNuevo->valorAlgoritmo = -1;

	queue_push(cPersonajes, pPersonajeNuevo);
}

void crearHiloPlanificador(pthread_t *pPlanificador, tNivel *nivelNuevo, t_list *lPlanificadores) {

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
    int iEnviarTurno;

    //Para multiplexar
    int iSocketConexion;
    fd_set readfds;
    FD_ZERO(&readfds);
    tMensaje tipoMensaje;
    tPersonaje* pPersonajeActual;
    char* sPayload;

    bool primerIntento = true;
    pPersonajeActual = NULL;
    iEnviarTurno	 = 0;

    // Ciclo donde se multiplexa para escuchar personajes que se conectan
    while (1) {

       	wait_personajes(&primerIntento);
        //si no hay personaje, se saca de la cola y se lo pone a jugar enviandole mensaje para jugar
       	seleccionarJugador(&pPersonajeActual, pNivel, iEnviarTurno);

        iSocketConexion = multiplexar(&pNivel->masterfds, &readfds, &pNivel->maxSock, &tipoMensaje, &sPayload, logger);

        if (iSocketConexion != -1) {
            pthread_mutex_lock(&semNivel);
            iEnviarTurno = 0;
            switch (tipoMensaje) {

            /* Mensajes que puede mandar el personaje */

            case(P_POS_RECURSO):
                posicionRecursoPersonaje(iSocketConexion, sPayload, pNivel->socket, logger);
                iEnviarTurno = 1;
                break;

            case(P_MOVIMIENTO):
                movimientoPersonaje(iSocketConexion, sPayload, pNivel, pPersonajeActual, logger);
                break;

            case(P_SIN_VIDAS):
                //personajeSinVidas(iSocketConexion, sPayload);
                break;

            case(P_DESCONECTARSE_MUERTE):
                //muertePersonaje(iSocketConexion, sPayload);
                break;

            case(P_DESCONECTARSE_FINALIZADO):
                //finalizacionPersonaje(iSocketConexion, sPayload);
                break;

            case(P_SOLICITUD_RECURSO):
                solicitudRecursoPersonaje(iSocketConexion, sPayload, pNivel, &pPersonajeActual, logger);
                break;

			/* Mensajes que puede mandar el nivel */

            case(N_ACTUALIZACION_CRITERIOS):
                actualizacionCriteriosNivel(iSocketConexion, sPayload, pNivel, logger);
                break;

            case(N_MUERTO_POR_ENEMIGO):
				muertePorEnemigoPersonaje(sPayload, pNivel, pPersonajeActual, logger);
            	break;

            case(N_ENTREGA_RECURSO):
				recepcionRecurso(pNivel, sPayload, logger);
            	iEnviarTurno = 1;
            	break;

            case(N_CONFIRMACION_MOV):
				confirmarMovimiento(pPersonajeActual, logger);
				iEnviarTurno = 1;
				break;

            case(DESCONEXION):
				desconectar(pNivel, &pPersonajeActual, iSocketConexion, sPayload);
				iEnviarTurno = 1;
				break;

            default:
            	iEnviarTurno = 0;
                break;
            }

            pthread_mutex_unlock(&semNivel);

        }
    }

    pthread_exit(NULL);
}

int seleccionarJugador(tPersonaje** pPersonaje, tNivel* nivel, int iEnviarTurno) {
    int iTamanioCola;
    // Me fijo si puede seguir jugando
    if (*pPersonaje != NULL) {
        switch(nivel->algoritmo) {
        case RR:
            if ((*pPersonaje)->valorAlgoritmo < nivel->quantum) {
            	log_debug(logger, "\n Quantum personaje: %d \n Quantum nivel: %d. \n Enviar Turno: %d.", (*pPersonaje)->valorAlgoritmo, nivel->quantum, iEnviarTurno);
                // Puede seguir jugando
            	if (iEnviarTurno == 1) {
            		enviarTurno(*pPersonaje);
            	}
                return (EXIT_SUCCESS);
            } else {
                // Termina su quantum vuelve a la cola
                queue_push(nivel->cListos, *pPersonaje);
            }
            break;

        case SRDF:
            if ((*pPersonaje)->valorAlgoritmo > 0) {
                // Puede seguir jugando
            	if (iEnviarTurno == 1) {
					enviarTurno(*pPersonaje);
				}
                return (EXIT_SUCCESS);
            } else if ((*pPersonaje)->valorAlgoritmo == 0) {
                // Llego al recurso, se bloquea
                return (EXIT_FAILURE);
            }
            break;
        }
    }

    // Busco al proximo personaje para darle turno
    iTamanioCola = queue_size(nivel->cListos);

    if (iTamanioCola == 0) {
        // La magia de cuando termina de planificar TODO
        pthread_exit(NULL);

    } else if (iTamanioCola == 1) {
        *pPersonaje = queue_pop(nivel->cListos);
        switch(nivel->algoritmo) {
		case RR:
			(*pPersonaje)->valorAlgoritmo = 0;
			break;
		case SRDF:
			break;
		}

    } else if (iTamanioCola > 1) {

        switch(nivel->algoritmo) {
        case RR:
            *pPersonaje = queue_pop(nivel->cListos);
            (*pPersonaje)->valorAlgoritmo = 0;
            break;
        case SRDF:
            *pPersonaje = planificacionSRDF(nivel->cListos, iTamanioCola);
            break;
        }
    }

    enviarTurno(*pPersonaje);

    return EXIT_SUCCESS;
}

void enviarTurno(tPersonaje *pPersonaje) {
	tPaquete pkgProximoTurno;
	pkgProximoTurno.type = PL_OTORGA_TURNO;
	pkgProximoTurno.length = 0;
	enviarPaquete(pPersonaje->socket, &pkgProximoTurno, logger, "Se otorga turno al personaje");
}

tPersonaje* planificacionSRDF(t_queue* cListos, int iTamanioCola) {
    int iNroPersonaje;
    int iDistanciaFaltanteMinima = 4000;
    tPersonaje *pPersonaje, *pPersonajeTemp;

    for (iNroPersonaje = 0; iNroPersonaje < iTamanioCola; iNroPersonaje ++) {
        pPersonajeTemp = list_get(cListos->elements, iNroPersonaje);

        if ((pPersonajeTemp->valorAlgoritmo > 0) && (pPersonajeTemp->valorAlgoritmo < iDistanciaFaltanteMinima)) {
            iDistanciaFaltanteMinima = pPersonajeTemp->valorAlgoritmo;
            pPersonaje = pPersonajeTemp;
        }
    }

    if (iDistanciaFaltanteMinima == 4000) { // Si no se encontro ninguno que cumpla los requisitos, agarro el primero de la cola
        pPersonaje = queue_pop(cListos);
        pPersonaje->valorAlgoritmo = -1; // -1 quiere decir que no tiene data
    }

    return pPersonaje;
}

void posicionRecursoPersonaje(int iSocketConexion, char* sPayload, int socketNivel, t_log* logger) {
    tMensaje tipoMensaje;
    tPaquete pkgPosRecurso;
    pkgPosRecurso.type   = PL_POS_RECURSO;
    pkgPosRecurso.length = strlen(sPayload);
    strcpy(pkgPosRecurso.payload, sPayload);

    enviarPaquete(socketNivel, &pkgPosRecurso, logger, "Solicitud al NIVEL la posicion de recurso");
    recibirPaquete(socketNivel, &tipoMensaje, &sPayload, logger, "Recibo del NIVEL posicion del recurso");

    if (tipoMensaje == N_POS_RECURSO) {
        pkgPosRecurso.type    = PL_POS_RECURSO;
        pkgPosRecurso.length  = strlen(sPayload);
        strcpy(pkgPosRecurso.payload, sPayload);

        enviarPaquete(iSocketConexion, &pkgPosRecurso, logger, "Envio de posicion de recurso al personaje");

    } else {
        pkgPosRecurso.type    = NO_SE_OBTIENE_RESPUESTA;
        pkgPosRecurso.length  = 0;

        enviarPaquete(iSocketConexion, &pkgPosRecurso, logger, "WARN: No se obtiene respuesta esperada del nivel");
    }

    free(sPayload);
}

void movimientoPersonaje(int iSocketConexion, char* sPayload, tNivel *pNivel, tPersonaje* pPersonaje, t_log* logger) {
    tPaquete pkgMovimientoPers;
    tMovimientoPers *movPers = deserializarMovimientoPers(sPayload);
    serializarMovimientoPers(PL_MOV_PERSONAJE, *movPers, &pkgMovimientoPers);

    enviarPaquete(pNivel->socket, &pkgMovimientoPers, logger, "Envio movimiento del personaje");

	if (pNivel->algoritmo == RR) {
		pPersonaje->valorAlgoritmo += 1;

	} else {
		pPersonaje->valorAlgoritmo -= 1;
	}
	free(sPayload);
	free(movPers);
}

void muertePorEnemigoPersonaje(char* sPayload, tNivel *pNivel, tPersonaje* pPersonaje, t_log* logger) {
	tPaquete pkgMuertePers;
	pkgMuertePers.type    = PL_MUERTO_POR_ENEMIGO;
	pkgMuertePers.length  = strlen(sPayload);

	/* TODO analizar bien que pasa cuando muere el personaje */

	enviarPaquete(pPersonaje->socket, &pkgMuertePers, logger, "Envio mensaje de muerte por personaje");
}

void solicitudRecursoPersonaje(int iSocketConexion, char *sPayload, tNivel *pNivel, tPersonaje **pPersonaje, t_log *logger) {
    tPaquete pkgSolicituRecurso;
    pkgSolicituRecurso.type    = PL_SOLICITUD_RECURSO;
    pkgSolicituRecurso.length  = strlen(sPayload);
    strcpy(pkgSolicituRecurso.payload, sPayload);

    tSimbolo *pSimbolo;
    pSimbolo = deserializarSimbolo(sPayload);
    log_info(logger, "El personaje %c solicita el recurso %c", (*pPersonaje)->simbolo, *pSimbolo);

    if (pNivel->algoritmo == RR) {
    	(*pPersonaje)->valorAlgoritmo = pNivel->quantum;
    } else {
    	(*pPersonaje)->valorAlgoritmo = 0;
    }

    tPersonajeBloqueado *pPersonajeBloqueado;
    pPersonajeBloqueado = (tPersonajeBloqueado*) malloc(sizeof(tPersonajeBloqueado));
    pPersonajeBloqueado->pPersonaje = *pPersonaje;
    pPersonajeBloqueado->recursoEsperado = *pSimbolo;

    list_add(pNivel->lBloqueados, pPersonajeBloqueado);

    *pPersonaje = NULL;
    free(pSimbolo);
    free(sPayload);

    enviarPaquete(pNivel->socket, &pkgSolicituRecurso, logger, "Solicitud de recurso");
}

void confirmarMovimiento(tPersonaje *pPersonajeActual, t_log *logger) {
	tPaquete pkgConfirmacionMov;
	pkgConfirmacionMov.type    = PL_CONFIRMACION_MOV;
	pkgConfirmacionMov.length  = 0;
	enviarPaquete(pPersonajeActual->socket, &pkgConfirmacionMov, logger, "Se envia confirmacion de movimiento al personaje");
}

int actualizacionCriteriosNivel(int iSocketConexion, char* sPayload, tNivel* pNivel, t_log* logger) {
	tInfoNivel* pInfoNivel;
	pInfoNivel = deserializarInfoNivel(sPayload);
	log_info(logger, "Se recibe nueva informacion del nivel");

	pNivel->quantum   = pInfoNivel->quantum;
	pNivel->delay     = pInfoNivel->delay;
	pNivel->algoritmo = pInfoNivel->algoritmo;

	free(sPayload);
	free(pInfoNivel);

	return EXIT_SUCCESS;
}

void recepcionRecurso(tNivel *pNivel, char *sPayload, t_log *logger) {
	tSimbolo *pSimbolo;
	pSimbolo = deserializarSimbolo(sPayload);
	log_info(logger, "Se recibe mensaje de liberacion de recurso");

	tPersonaje *pPersonaje;
	pPersonaje = desbloquearPersonaje(pNivel->lBloqueados, *pSimbolo);

	if (pPersonaje != NULL) {
		log_info(logger, "Se desbloquea el personaje: %c", pPersonaje->simbolo);
		queue_push(pNivel->cListos, pPersonaje);
		log_info(logger, "Se mueve al personaje: %c a la cola de listos", pPersonaje->simbolo);

	} else {
		log_info(logger, "No se encontro ningun personaje esperando por el recurso");
	}
	free(pSimbolo);
	free(sPayload);

}

void desconectar(tNivel *pNivel, tPersonaje **pPersonajeActual, int iSocketConexion, char *sPayload) {
	tPersonaje *pPersonaje;
	log_info(logger, "Se detecta desconexion...");
	/*UNDER CONSTRUCTION*/

	if (iSocketConexion == (*pPersonajeActual)->socket) {
		log_info(logger, "Se desconecto el personaje %c", (*pPersonajeActual)->simbolo);
		/* Acá hay que liberar recursos, y asignarselos a los que estan en la listas de bloqueados */
		tPaquete pkgDesconexionPers;
		serializarSimbolo(PL_DESCONEXION_PERSONAJE, (*pPersonajeActual)->simbolo, &pkgDesconexionPers);
		enviarPaquete(pNivel->socket, &pkgDesconexionPers, logger, "Se envia desconexion del personaje actual al nivel");
		free(*pPersonajeActual);
		*pPersonajeActual = NULL;
	}

	pPersonaje = sacarPersonajeDeListas(pNivel, iSocketConexion);
	log_info(logger, "Se desconecto el personaje %c", pPersonaje->simbolo);
	if (pPersonaje != NULL) {
		log_info(logger, "Se desconecto el personaje %c", pPersonaje->simbolo);
		/* Acá hay que liberar recursos, y asignarselos a los que estan en la listas de bloqueados */
		tPaquete pkgDesconexionPers;
		serializarSimbolo(PL_DESCONEXION_PERSONAJE, pPersonaje->simbolo, &pkgDesconexionPers);
		enviarPaquete(pNivel->socket, &pkgDesconexionPers, logger, "Se envia desconexion del personaje al nivel");
		free(pPersonaje);
	}
	else{
		log_error(logger, "ERROR: no se encontró el personaje que salio en socket %d", iSocketConexion);
		exit(EXIT_FAILURE);
	}
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
				return iNivelLoop;
			}
		}
	}

	return -1;
}

/*
 * Verificar si el personaje existe, si existe devuelve el indice de su posicion, sino devuelve -1
 */
int existePersonaje(t_list *pListaPersonajes, int iSocket) {
	int iPersonajeLoop;
	int iCantPersonajes = list_size(pListaPersonajes);
	tPersonaje* pPersonajeGuardado;

	if (iCantPersonajes > 0) {
		for (iPersonajeLoop = 0; iPersonajeLoop < iCantPersonajes; iPersonajeLoop++) {
			pPersonajeGuardado = (tPersonaje *)list_get(pListaPersonajes, iPersonajeLoop);

			if (pPersonajeGuardado->socket == iSocket) {
				return iPersonajeLoop;
			}
		}
	}

	return -1;
}

/*
 * Elimina al personaje que contiene el socket pasado por parametro de todas las listas del nivel y lo devuelve, si no lo encuentra devuelve null
 */
tPersonaje *sacarPersonajeDeListas(tNivel *pNivel, int iSocket) {
	int iIndicePersonaje;

	iIndicePersonaje = existePersonaje(pNivel->cListos->elements, iSocket);

	if (iIndicePersonaje != -1) {
		return (list_remove(pNivel->cListos->elements, iIndicePersonaje)); // Y sacamos al personaje de la lista de bloqueados
	}

	iIndicePersonaje = existePersonaje(pNivel->lBloqueados, iSocket);

	if (iIndicePersonaje != -1) {
		return (list_remove(pNivel->lBloqueados, iIndicePersonaje));
	}

	return NULL;
}

void imprimirLista(tNivel *pNivel, tPersonaje *pPersonaje) {

	char* tmp = malloc(30);
	char* retorno = malloc(500);
	int i;
	tPersonaje *pPersAux;

	if (queue_is_empty(pNivel->cListos) && (pPersonaje == NULL)) { //Si no hay nadie listo, no se quien esta ejecutando
		sprintf(retorno, "Lista de: %s\n\tEjecutando:\n\tListos: \t", pNivel->nombre);
	} else {
		sprintf(retorno, "Lista de: %s\n\tEjecutando: %c\n\tListos: \t", pNivel->nombre, pPersonaje->simbolo);
	}

	int iCantidadListos = queue_size(pNivel->cListos);
	for (i = 0; i < iCantidadListos; i++) {
		pPersAux = (tPersonaje *)queue_peek(pNivel->cListos);
		sprintf(tmp, "%c -> ", pPersAux->simbolo);
		string_append(&retorno, tmp);
	}

	sprintf(tmp, "\n\tBloqueados: \t");
	string_append(&retorno, tmp);

	int iCantidadBloqueados = list_size(pNivel->lBloqueados);
	for (i = 0; i < iCantidadBloqueados; i++) {
		pPersAux = (tPersonaje *) list_get(pNivel->lBloqueados, i);
		sprintf(tmp, "%c -> ", pPersAux->simbolo);
		string_append(&retorno, tmp);
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
tPersonaje* desbloquearPersonaje(t_list* lBloqueados, tSimbolo recurso) {
	tPersonajeBloqueado *pPersonajeBloqueado;
	tPersonaje* pPersonaje;
	int iCantidadBloqueados;
	int iIndicePersonaje;
	int bEncontrado = 0;

	iCantidadBloqueados = list_size(lBloqueados);

	for (iIndicePersonaje = 0; (iIndicePersonaje < iCantidadBloqueados) && (bEncontrado == 0); iIndicePersonaje++) {
		pPersonajeBloqueado = (tPersonajeBloqueado *)list_get(lBloqueados, iIndicePersonaje);
		bEncontrado    		= (pPersonajeBloqueado->recursoEsperado == recurso);
	}

	if (bEncontrado == 1) {
		pPersonajeBloqueado = list_remove(lBloqueados, iIndicePersonaje);
		pPersonaje = pPersonajeBloqueado->pPersonaje;
		tSimbolo *recursoObtenido;
		recursoObtenido  = (tSimbolo *) malloc(sizeof(tSimbolo));
		*recursoObtenido = pPersonajeBloqueado->recursoEsperado;
		list_add(pPersonaje->recursos, recursoObtenido);
		free(pPersonajeBloqueado);

		return pPersonaje;

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
		log_warning(logger, "WARN: Error al delegar conexiones");
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

void imprimirConexiones(fd_set *master_planif, int maxSock, char* host) {
	int i;
	int cantSockets =0;

	log_debug(logger, "Conexiones del %s", host);
	for(i = 0; i<=maxSock; i++) {
		if(FD_ISSET(i, master_planif)) {
			log_debug(logger, "El socket %d esta en el conjunto", i);
			cantSockets++;
		}
	}
	log_debug(logger, "La cantidad de sockets totales del %s es %d", host, cantSockets);
}

void signal_personajes() {
	pthread_cond_signal(&hayPersonajes);
}

void wait_personajes(bool *primerIntento) {

	if (*primerIntento) {
		pthread_mutex_lock(&semNivel);
		pthread_cond_wait(&hayPersonajes, &semNivel);
		pthread_mutex_unlock(&semNivel);
	}
	*primerIntento = false;
}

tNivel *getNivel(char *nom_nivel){
	int i;
	int size_niveles = list_size(listaNiveles);
	for(i = 0; 0 < size_niveles; i++){
		tNivel *nivel = list_get(listaNiveles, i);
		if(string_equals_ignore_case(nivel->nombre, nom_nivel))
			return nivel;
	}
	return NULL;
}

