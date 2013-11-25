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
pthread_t hilo_orquestador;
t_list 	 *listaNiveles;
t_config *configPlataforma;
t_log *logger;
unsigned short usPuerto;
char *pathKoopa;
char *pathScript;
bool hay_personajes;


/*
 * Funciones privadas
 */

int seleccionarJugador(tPersonaje* pPersonaje, tNivel* nivel);


/*
 * Funciones privadas orquestador
 */

int conexionNivel(int iSocketComunicacion, char* sPayload, fd_set* socketsOrquestador, t_list *lPlanificadores);
int conexionPersonaje(int iSocketComunicacion, fd_set* socketsOrquestador, char* sPayload);
void crearHiloPlanificador(pthread_t *pPlanificador, tNivel *nivelNuevo, t_list *lPlanificadores);

/*
 * Funciones privadas planificador
 */

int seleccionarJugador(tPersonaje* pPersonaje, tNivel* nivel);
tPersonaje* planificacionSRDF(t_queue* cListos, int iTamanioCola);
void solicitudRecursoPersonaje(int iSocketConexion, char *sPayload, tNivel *pNivel, tPersonaje **pPersonajeActual, t_log *logger);
void movimientoPersonaje(int iSocketConexion, char* sPayload, tNivel *pNivel, tPersonaje* pPersonaje, t_log* logger);
void posicionRecursoPersonaje(int iSocketConexion, char* sPayload, tNivel* pNivel, t_log* logger);
int actualizacionCriteriosNivel(int iSocketConexion, char* sPayload, tNivel* pNivel);

/*
 * PLATAFORMA
 */

int main(int argc, char*argv[]) {

	if (argc <= 1) {
		printf("[ERROR] Se debe pasar como parametro el archivo de configuracion.\n");
		exit(EXIT_FAILURE);
	}

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

	orquestador(usPuerto);
//	pthread_create(&hilo_orquestador, NULL, orquestador, NULL );
//
//	pthread_join(hilo_orquestador, NULL );

	return EXIT_SUCCESS;
}

/*
 * ORQUESTADOR
 */

void *orquestador(unsigned short usPuerto) {
	t_list *lPlanificadores;

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

	return NULL;
}

int conexionNivel(int iSocketComunicacion, char* sPayload, fd_set* socketsOrquestador, t_list *lPlanificadores) {

	int iIndiceNivel;
	iIndiceNivel = existeNivel(listaNiveles, sPayload);

	if (iIndiceNivel < 0) {
		tPaquete pkgNivelRepetido;
		pkgNivelRepetido.type   = PL_NIVEL_YA_EXISTENTE;
		pkgNivelRepetido.length = 0;
		enviarPaquete(iSocketComunicacion, &pkgNivelRepetido, logger, "Ya se encuentra conectado al orquestador un nivel con el mismo nombre");
		return EXIT_FAILURE;
	}

	pthread_t *pPlanificador;
	tMensaje tipoMensaje;
	tInfoNivel *pInfoNivel;

	tNivel *nivelNuevo = (tNivel *) malloc(sizeof(tNivel));
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

		crearNivel(listaNiveles, nivelNuevo, iSocketComunicacion, sNombreNivel, pInfoNivel);
		crearHiloPlanificador(pPlanificador, nivelNuevo, lPlanificadores);

		inicializarConexion(&nivelNuevo->masterfds, &nivelNuevo->maxSock, &iSocketComunicacion);
		delegarConexion(&nivelNuevo->masterfds, socketsOrquestador, &iSocketComunicacion, &nivelNuevo->maxSock);

		// Logueo el nuevo hilo recien creado
		log_debug(logger, "Nuevo planificador del nivel: '%s' y planifica con: %s", nivelNuevo->nombre, nivelNuevo->algoritmo);

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

	iIndiceNivel = existeNivel(listaNiveles, pHandshakePers->nombreNivel);

	if (iIndiceNivel >= 0) {
		tNivel *pNivelPedido = (tNivel*) malloc(sizeof(tNivel));
		pNivelPedido = list_get(listaNiveles, iIndiceNivel);

		// Logueo del pedido de nivel del personaje
		log_trace(logger, "Se conectó el personaje %c. Pide nivel: %s", pHandshakePers->simbolo, pHandshakePers->nombreNivel);

		tPaquete pkgHandshake;
		pkgHandshake.type   = PL_HANDSHAKE;
		pkgHandshake.length = 0;

		delegarConexion(&pNivelPedido->masterfds, socketsOrquestador, &iSocketComunicacion, &pNivelPedido->maxSock);
		agregarPersonaje(pNivelPedido->cListos, pHandshakePers->simbolo, iSocketComunicacion);
		signal_personajes(&pNivelPedido->hay_personajes);

		// Le contesto el handshake
		enviarPaquete(iSocketComunicacion, &pkgHandshake, logger, "Handshake de la plataforma al personaje");
		free(pNivelPedido);

	} else {
		log_error(logger, "El nivel solicitado no se encuentra conectado a la plataforma");
		tPaquete pkgNivelInexistente;
		pkgNivelInexistente.type   = PL_NIVEL_INEXISTENTE;
		pkgNivelInexistente.length = 0;
		enviarPaquete(iSocketComunicacion, &pkgNivelInexistente, logger, "No se encontro el nivel pedido");

		return EXIT_FAILURE;
	}

	free(sPayload);
	free(pHandshakePers);
	return EXIT_SUCCESS;
}

void crearNivel(t_list* lNiveles, tNivel* nivelNuevo, int socket, char *levelName, tInfoNivel *pInfoNivel) {
	nivelNuevo->nombre = malloc(strlen(levelName) + 1);
	strcpy(nivelNuevo->nombre, levelName);
	nivelNuevo->cListos 	= queue_create();
	nivelNuevo->cBloqueados = queue_create();
	nivelNuevo->lMuertos 	= list_create();
	nivelNuevo->socket 		= socket;
	nivelNuevo->quantum 	= pInfoNivel->quantum;
	nivelNuevo->algoritmo 	= pInfoNivel->algoritmo;
	nivelNuevo->delay 		= pInfoNivel->delay;
	nivelNuevo->maxSock 	= 0;

	list_add(lNiveles, nivelNuevo);
}


void agregarPersonaje(t_queue *cPersonajes, tSimbolo simbolo, int socket) {
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
    tNivel* pNivel;
    pNivel = (tNivel*) pvNivel;

    //Para multiplexar
    int iSocketConexion;
    fd_set readfds;
    FD_ZERO(&readfds);
    tMensaje tipoMensaje;
    tPersonaje* pPersonajeActual;
    char* sPayload;

    pPersonajeActual = NULL;
    // Ciclo donde se multiplexa para escuchar personajes que se conectan
    while (1) {

        wait_personajes(&pNivel->hay_personajes);
        //si no hay personaje, se saca de la cola y se lo pone a jugar enviandole mensaje para jugar
        seleccionarJugador(pPersonajeActual, pNivel);

        iSocketConexion = multiplexar(&pNivel->masterfds, &readfds, &pNivel->maxSock, &tipoMensaje, &sPayload, logger);

        if (iSocketConexion != -1) {

            pthread_mutex_lock(&semNivel);

            switch (tipoMensaje) {
            case(P_POS_RECURSO):
                posicionRecursoPersonaje(iSocketConexion, sPayload, pNivel, logger);
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

            case(N_ACTUALIZACION_CRITERIOS):
                actualizacionCriteriosNivel(iSocketConexion, sPayload, pNivel);
                break;

            default:
                break;
            }

            pthread_mutex_unlock(&semNivel);

        }
    }
}

int seleccionarJugador(tPersonaje* pPersonaje, tNivel* nivel) {

    int iTamanioCola;

    // Me fijo si puede seguir jugando
    if (pPersonaje == NULL) {
        switch(nivel->algoritmo) {
        case RR:
            if (pPersonaje->valorAlgoritmo < nivel->quantum) {
                // Puede seguir jugando
                return (EXIT_SUCCESS);
            } else {
                // Termina su quantum cuelve a la cola
                queue_push(nivel->cListos, pPersonaje);
            }
            break;

        case SRDF:
            if (pPersonaje->valorAlgoritmo > 0) {
                // Puede seguir jugando
                return (EXIT_SUCCESS);
            } else if (pPersonaje->valorAlgoritmo == 0) {
                // Llego al recurso, se bloquea
                queue_push(nivel->cBloqueados, pPersonaje);
            }
            break;
        }
    }

    // Busco al proximo personaje para darle turno
    iTamanioCola = queue_size(nivel->cListos);

    if (iTamanioCola == 0) {
        // La magia de cuando termina de planificar
        pthread_exit(NULL);

    } else if (iTamanioCola == 1) {
        pPersonaje = queue_pop(nivel->cListos);

    } else if (iTamanioCola > 1) {

        switch(nivel->algoritmo) {
        case RR:
            pPersonaje = queue_pop(nivel->cListos);
            break;
        case SRDF:
            pPersonaje = planificacionSRDF(nivel->cListos, iTamanioCola);
            break;
        }
    }

    tPaquete pkgProximoTurno;
    pkgProximoTurno.type = PL_OTORGA_TURNO;
    pkgProximoTurno.length = 0;
    enviarPaquete(pPersonaje->socket, &pkgProximoTurno, logger, "Se otorga turno");

    return EXIT_SUCCESS;
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

void posicionRecursoPersonaje(int iSocketConexion, char* sPayload, tNivel* pNivel, t_log* logger) {
    tMensaje tipoMensaje;
    tPaquete pkgPosRecurso;
    pkgPosRecurso.type    = PL_POS_RECURSO;
    pkgPosRecurso.length  = strlen(sPayload);
    strcpy(pkgPosRecurso.payload, sPayload);
    free(sPayload);

    enviarPaquete(pNivel->socket, &pkgPosRecurso, logger, "Solicitud posicion de recurso");
    recibirPaquete(pNivel->socket, &tipoMensaje, &sPayload, logger, "Recibo posicion del recurso");

    if (tipoMensaje == N_POS_RECURSO) {
        pkgPosRecurso.type    = PL_POS_RECURSO;
        pkgPosRecurso.length  = strlen(sPayload);
        strcpy(pkgPosRecurso.payload, sPayload);

        enviarPaquete(iSocketConexion, &pkgPosRecurso, logger, "Envio de posicion de recurso al personaje");

    } else {
        pkgPosRecurso.type    = NO_SE_OBTIENE_RESPUESTA;
        pkgPosRecurso.length  = 0;

        enviarPaquete(iSocketConexion, &pkgPosRecurso, logger, "No se obtiene respuesta esperada del nivel");
    }

    free(sPayload);
}

void movimientoPersonaje(int iSocketConexion, char* sPayload, tNivel *pNivel, tPersonaje* pPersonaje, t_log* logger) {
    tMensaje tipoMensaje;
    tPaquete pkgMovimientoPers;
    pkgMovimientoPers.type    = PL_MOV_PERSONAJE;
    pkgMovimientoPers.length  = strlen(sPayload);
    strcpy(pkgMovimientoPers.payload, sPayload);
    free(sPayload);

    enviarPaquete(pNivel->socket, &pkgMovimientoPers, logger, "Envio movimiento del personaje");

    recibirPaquete(pNivel->socket, &tipoMensaje, &sPayload, logger, "Recibo estado en el que quedo el personaje");

    if (tipoMensaje == N_MUERTO_POR_ENEMIGO) { // TODO verificar que mensajes puede recibir
//        tPaquete pkgMovimientoPers;
//        pkgMovimientoPers.type    = PL_MUERTO_POR_ENEMIGO;
//        pkgMovimientoPers.length  = strlen(sPayload);

        recibirPaquete(pNivel->socket, &tipoMensaje, &sPayload, logger, "Envio muerte por enemigo");

        //Limpio la lista de recursos que tenia hasta que se murio
        list_clean(pPersonaje->recursos);

        //Lo agrego a la cola de finalizados y lo saco cuando manda msj en el case SALUDO
        list_add(pNivel->lMuertos, pNivel);

    } else {

        if (pNivel->algoritmo == RR) {
            pPersonaje->valorAlgoritmo += 1;

        } else {
            pPersonaje->valorAlgoritmo -= 1;
        }
    }

}


void solicitudRecursoPersonaje(int iSocketConexion, char *sPayload, tNivel *pNivel, tPersonaje **pPersonajeActual, t_log *logger) {
    tPaquete pkgSolicituRecurso;
    pkgSolicituRecurso.type    = PL_SOLICITUD_RECURSO;
    pkgSolicituRecurso.length  = strlen(sPayload);
    strcpy(pkgSolicituRecurso.payload, sPayload);

    enviarPaquete(pNivel->socket, &pkgSolicituRecurso, logger, "Solicitud de recurso");

    if (pNivel->algoritmo == RR) {
    	(*pPersonajeActual)->valorAlgoritmo = pNivel->quantum;
    } else {
    	(*pPersonajeActual)->valorAlgoritmo = 0;
    }

    queue_push(pNivel->cBloqueados, *pPersonajeActual);

    *pPersonajeActual = NULL;
}


int actualizacionCriteriosNivel(int iSocketConexion, char* sPayload, tNivel* pNivel) {
	tInfoNivel* pInfoNivel;
	pInfoNivel = deserializarInfoNivel(sPayload);

	pNivel->quantum   = pInfoNivel->quantum;
	pNivel->delay     = pInfoNivel->delay;
	pNivel->algoritmo = pInfoNivel->algoritmo;

	free(sPayload);
	free(pInfoNivel);

	return EXIT_SUCCESS;
}


/*
 * Verificar si el nivel existe, si existe devuelve el indice de su posicion, sino devuelve -1
 */
int existeNivel(t_list * lNiveles, char* sLevelName) {
	int iNivelLoop, bEncontrado;
	int iCantNiveles = list_size(lNiveles);
	tNivel* pNivelGuardado;

	if (!list_is_empty(lNiveles)) {

		for (iNivelLoop = 0; (iNivelLoop < iCantNiveles) && (bEncontrado == 0); iNivelLoop++) {
			pNivelGuardado = (tNivel *)list_get(listaNiveles, iNivelLoop);
			bEncontrado    = (strcmp (pNivelGuardado->nombre, sLevelName) == 0);
		}

		if (bEncontrado == 1) {
			return iNivelLoop;
		}
	}

	return -1;
}

int buscarPersonajePorSocket(t_list *lista, int socket, tPersonaje *personaje){
	int contPj;

	for (contPj =0 ; contPj<list_size(lista); contPj++) {
		personaje = list_get(lista, contPj);

		if(personaje->socket == socket) {
			return contPj;
		}
	}

	return -1;
}

bool sacarPersonajeDeListas(t_list *ready, t_list *block, int socket,  tPersonaje *pjLevantador) {
	int indice_personaje;
	bool encontrado = false;

	indice_personaje = buscarPersonajePorSocket(block, socket, pjLevantador);

	if(indice_personaje != -1){
		encontrado = true; //Si lo encontras en Blk, cambiamos el flag
		pjLevantador = list_remove(block, indice_personaje); // Y sacamos al personaje de la lista de bloqueados
	}
	//Si no lo encuentra: lo busco en la lista de Ready y lo saco de la misma
	if (!encontrado) { //Si no lo encontras, buscarlo en rdy
		indice_personaje = buscarPersonajePorSocket(ready, socket, pjLevantador);
		pjLevantador = list_remove(ready, indice_personaje); // Lo sacamos de la lista
	}

	return encontrado;
}

//void imprimirLista(tNivel *pNivel, tPersonaje *pPersonaje) {
//
//	char* tmp;
//	char* retorno;
//	int i;
//	tPersonaje *pPersAux;
//
//	if (queue_is_empty(pNivel->cListos) && (pPersonaje == NULL)) { //Si no hay nadie listo, no se quien esta ejecutando
//		asprintf(&retorno, "Lista de: %s\n\tEjecutando:\n\tListos: \t", pNivel->nombre);
//	} else {
//		asprintf(&retorno, "Lista de: %s\n\tEjecutando: %c\n\tListos: \t", pNivel->nombre, pPersonaje->simbolo);
//	}
//
//	int iCantidadListos = queue_size(pNivel->cListos);
//	for (i = 0; i < iCantidadListos; i++) {
//		pPersAux = (tPersonaje *)list_get_data(pNivel->cListos->elements, i);
//		asprintf(&tmp, "%c -> ", pPersAux->simbolo);
//		string_append(&retorno, tmp);
//		free(tmp);
//	}
//
//	asprintf(&tmp, "\n\tBloqueados: \t");
//	string_append(&retorno, tmp);
//	free(tmp);
//
//	int iCantidadBloqueados = queue_size(pNivel->cBloqueados);
//	for (i = 0; i < iCantidadBloqueados; i++) {
//		pPersAux = list_get(pNivel->cListos->elements, i);
//		asprintf(&tmp, "%c -> ", pPersAux->simbolo);
//		string_append(&retorno, tmp);
//		free(tmp);
//	}
//
//	log_info(logger, retorno);
//	free(tmp);
//	free(retorno);
//}

void delegarConexion(fd_set *master_planif, fd_set *master_orq, int *sock, int *maxSock) {
	//Saco el socket del conjunto de sockets del orquestador
	FD_CLR(*sock, master_orq);
	FD_SET(*sock, master_planif);

	//Lo agrego al conjunto del planificador
	if (FD_ISSET(*sock, master_planif)) {
		log_debug(logger, "--> Delegue la conexion del personaje al planificador <--");

	} else {
		log_warning(logger, "WARN: Error al delegar conexiones");
		exit(EXIT_FAILURE);
	}

	//Actualizo el tope del set de sockets
	if (*sock > *maxSock) {
		*maxSock = *sock;
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


void signal_personajes(bool *hay_personajes) {
	*hay_personajes = true;
	pthread_cond_signal(&hayPersonajes);
}


void wait_personajes(bool *hay_personajes) {

	if (!(*hay_personajes)) {
		pthread_mutex_lock(&semNivel);
		pthread_cond_wait(&hayPersonajes, &semNivel);
		pthread_mutex_unlock(&semNivel);
	}
}

