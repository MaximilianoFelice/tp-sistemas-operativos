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

fd_set setSocketsOrquestador;
int iSocketMaximoOrquestador;
char *pathKoopa;
char *pathScript;

/*
 * Funciones privadas
 */
tPersonaje* desbloquearPersonaje(t_list* lBloqueados, tSimbolo recurso);
tPersonaje *sacarPersonajeDeListas(tNivel *pNivel, int iSocket);
char *getRecursosNoAsignados(t_list *recursos);

/*
 * Funciones privadas plataforma
 */
int executeKoopa(char *koopaPath, char *scriptPath);
void orquestadorTerminaJuego();
bool nivelVacio(tNivel *);
void cerrarTodoSignal();
void cerrarTodo();

/*
 * Funciones privadas orquestador
 */
int conexionNivel(int iSocketComunicacion, char* sPayload, fd_set* pSetSocketsOrquestador, t_list *lPlanificadores);
int conexionPersonaje(int iSocketComunicacion, fd_set* socketsOrquestador, char* sPayload);
bool avisoConexionANivel(int sockNivel,char *sPayload, tSimbolo simbolo);
void sendConnectionFail(int sockPersonaje, tMensaje typeMsj, char *msjInfo);
void crearHiloPlanificador(pthread_t *pPlanificador, tNivel *nivelNuevo, t_list *lPlanificadores);


/*
 * Funciones privadas planificador
 */
int seleccionarJugador(tPersonaje** pPersonaje, tNivel* nivel, bool iEnviarTurno);
tPersonaje* planificacionSRDF(t_queue* cListos);
void solicitudRecursoPersonaje(int iSocketConexion, char *sPayload, tNivel *pNivel, tPersonaje **pPersonajeActual, t_log *logger);
void movimientoPersonaje(int iSocketConexion, char* sPayload, tNivel *pNivel, tPersonaje* pPersonaje, t_log* logger);
void posicionRecursoPersonaje(tNivel *pNivel, tPersonaje *personajeActual, int iSocketConexion, char* sPayload, int socketNivel, t_log* logger);
int actualizacionCriteriosNivel(int iSocketConexion, char* sPayload, tNivel* pNivel, t_log* logger);
void muertePorEnemigoPersonaje(char* sPayload, tNivel *pNivel, tPersonaje* pPersonaje, t_log* logger);
void recepcionRecurso(tNivel *pNivel, char *sPayload, t_log *logger);
void confirmarMovimiento(tNivel *nivel, tPersonaje *pPersonajeActual);
void matarPersonajeDeadlockeado(tNivel *pNivel, char *sPayload);

int desconectar(tNivel *pNivel, tPersonaje **pPersonajeActual, int iSocketConexion, char *sPayload);
char *liberarRecursos(tPersonaje *pPersMuerto, tNivel *pNivel);


void enviarTurno(tPersonaje *pPersonaje, int delay);

//Para manejar los turnos en el planificador
bool coordinarAccesoMultiplesPersonajes(tPersonaje *personajeActual, int socketConexion, bool valor);
bool esElPersonajeQueTieneElTurno(int socketActual, int socketConexion);

/*
 * PLATAFORMA
 */

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

//Con esto mato a todos los niveles... y de paso tmb a los personajes(que deberia estar cerrados)
void cerrarTodoSignal() {
	printf("\n");
	system("killall -SIGKILL personaje >/dev/null 2>&1");
	system("killall -SIGINT nivel >/dev/null 2>&1");
	printf("Process Killed.\n");
	exit(EXIT_FAILURE);
}

void cerrarTodo() {
	system("killall -SIGINT nivel &> /dev/null"); //No me digas si no hay nada!
	system("killall -SIGINT personaje &> /dev/null");
	log_trace(logger, "Proceso Finalizado.");
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
	FD_ZERO(&setSocketsOrquestador);
	int iSocketEscucha, iSocketComunicacion;

	// Inicializacion de sockets
	iSocketEscucha = crearSocketEscucha(usPuerto, logger);
	FD_SET(iSocketEscucha, &setSocketsOrquestador);
	iSocketMaximoOrquestador = iSocketEscucha;

	tMensaje tipoMensaje;
	char * sPayload;

	struct timeval timeout;
	timeout.tv_sec = 5;
	timeout.tv_usec = 0;
	//TODO
	//le puse un timeout porque cuando el planificador delega la conexion del personaje finalizado, si no es de esta forma se queda bloqueado
	//Por este motivo agregue el wait_personaje() y signal_personajes() en el planificador
	//Lo hacia de esa forma o con un timeout, para que no se quede bloqueado
	//Ya que el select va a checkear el set temporal de sockets y aunque yo le agregue un socket el select sigue mirando
	//La temporal, no importa que modifique tambien la temporal, osea aunque modifique el set master o temporal
	//Que usa el select lo mismo se queda bloqueado porque no detecta un evento ni nada.
	//Entonces con el timeout o con el wait_personajes() me evitaba eso.

	while (1) {
		iSocketComunicacion = getConnectionTimeOut(&setSocketsOrquestador, &iSocketMaximoOrquestador, iSocketEscucha, &tipoMensaje, &sPayload, &timeout, logger);

		if (iSocketComunicacion != -1) {

			pthread_mutex_lock(&semNivel);
	
			switch (tipoMensaje) {
			case N_HANDSHAKE: // Un nuevo nivel se conecta
				conexionNivel(iSocketComunicacion, sPayload, &setSocketsOrquestador, lPlanificadores);
				break;

			case P_HANDSHAKE:
				conexionPersonaje(iSocketComunicacion, &setSocketsOrquestador, sPayload);
				break;

			case DESCONEXION:
				log_debug(logger, "ORQUESTADOR: Me llego la desconexion en %d.....", iSocketComunicacion);
				orquestadorTerminaJuego();
				break;

			default:
				break;
			}

			pthread_mutex_unlock(&semNivel);

		}

	}

	pthread_exit(NULL);
}

void orquestadorTerminaJuego(){
	int indiceNivel;
	int cantidadNiveles = list_size(listaNiveles);
	bool noHayNadie = true;
	tNivel *nivelLevantador;
	log_debug(logger, "Verificando niveles...");
	if(list_size(listaNiveles) == 0){
		noHayNadie=false;
	}

	for(indiceNivel=0; indiceNivel < cantidadNiveles; indiceNivel++){
		nivelLevantador = list_get(listaNiveles, indiceNivel);
		if(!nivelVacio(nivelLevantador)){
			log_debug(logger, "El %s todavia tiene chaboncitos", nivelLevantador->nombre);
			noHayNadie = false;
			break;
		}
		log_debug(logger, "El %s esta vacio", nivelLevantador->nombre);
	}
	if(noHayNadie){
		log_debug(logger, "No hay tipitos jugando entonces ejecuto koopa y cierro todo");
		cerrarTodo();
		executeKoopa(pathKoopa, pathScript);
		exit(EXIT_FAILURE);
	}
}

bool nivelVacio(tNivel* nivel){
	return ((list_size(nivel->cListos->elements)==0) && (list_size(nivel->lBloqueados) == 0));
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
/*
		tSimbolo simboloMsj = '#';
		tPaquete *paquete = malloc(sizeof(tPaquete));
		serializarSimbolo(PL_CONEXION_PERS, simboloMsj, paquete);
		enviarPaquete(iSocketComunicacion, paquete, logger, "Envio al nivel el nuevo personaje que se conecto");
		simboloMsj = '=';
		serializarSimbolo(PL_CONEXION_PERS, simboloMsj, paquete);
		enviarPaquete(iSocketComunicacion, paquete, logger, "Envio al nivel el nuevo personaje que se conecto");
		simboloMsj = '?';
		serializarSimbolo(PL_CONEXION_PERS, simboloMsj, paquete);
		enviarPaquete(iSocketComunicacion, paquete, logger, "Envio al nivel el nuevo personaje que se conecto");
		sleep(2);
		tMovimientoPers *movPers=malloc(sizeof(tMovimientoPers)) ;
		int i;
		for(i=0;i<20;i++){
			movPers->simbolo='#';
			movPers->direccion=abajo;
			serializarMovimientoPers(PL_MOV_PERSONAJE, *movPers, paquete);
			enviarPaquete(iSocketComunicacion, paquete, logger, "Envio al nivel el movimiento del personaje");
			movPers->simbolo='=';
			movPers->direccion=derecha;
			serializarMovimientoPers(PL_MOV_PERSONAJE, *movPers, paquete);
			enviarPaquete(iSocketComunicacion, paquete, logger, "Envio al nivel el movimiento del personaje");
			usleep(780000);
		}
		movPers->simbolo='?';
		movPers->direccion=derecha;
		serializarMovimientoPers(PL_MOV_PERSONAJE, *movPers, paquete);
		enviarPaquete(iSocketComunicacion, paquete, logger, "Envio al nivel el movimiento del personaje");
		for(i=0;i<19;i++){
			movPers->simbolo='#';
			movPers->direccion=arriba;
			serializarMovimientoPers(PL_MOV_PERSONAJE, *movPers, paquete);
			enviarPaquete(iSocketComunicacion, paquete, logger, "Envio al nivel el movimiento del personaje");
			usleep(500000);
		}
		sleep(45);
		free(paquete);
		*/
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
			agregarPersonaje(pNivelPedido, pHandshakePers->simbolo, iSocketComunicacion);
			delegarConexion(&pNivelPedido->masterfds, socketsOrquestador, iSocketComunicacion, &pNivelPedido->maxSock);

			signal_personajes();

			tPaquete pkgHandshake;
			pkgHandshake.type   = PL_HANDSHAKE;
			pkgHandshake.length = 0;
			// Le contesto el handshake
			enviarPaquete(iSocketComunicacion, &pkgHandshake, logger, "Handshake de la plataforma al personaje");
		} else {
			log_error(logger, "El personaje ya esta jugando actualmente en ese nivel");
			sendConnectionFail(iSocketComunicacion, PL_PERSONAJE_REPETIDO,"El personaje ya esta jugando ese nivel");
			return EXIT_FAILURE;
		}

	} else {
		log_error(logger, "El nivel solicitado no se encuentra conectado a la plataforma");
		sendConnectionFail(iSocketComunicacion, PL_NIVEL_INEXISTENTE, "No se encontro el nivel pedido");
		return EXIT_FAILURE;
	}

	free(sPayload);
	free(pHandshakePers);
	return EXIT_SUCCESS;
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
	pNivelNuevo->socket 	 = socket;
	pNivelNuevo->quantum 	 = pInfoNivel->quantum;
	pNivelNuevo->algoritmo 	 = pInfoNivel->algoritmo;
	pNivelNuevo->delay 		 = pInfoNivel->delay;
	pNivelNuevo->maxSock 	 = socket;
	pNivelNuevo->cantidadIngresantes = 0;
	FD_ZERO(&(pNivelNuevo->masterfds));

	list_add(lNiveles, pNivelNuevo);
}

void agregarPersonaje(tNivel *pNivel, tSimbolo simbolo, int socket) {

	log_debug(logger, "Apunto de agregar a un personaje");
	tPersonaje *pPersonajeNuevo;
	pPersonajeNuevo = malloc(sizeof(tPersonaje));

	pPersonajeNuevo->simbolo  	  	= simbolo;
	pPersonajeNuevo->socket 	  	= socket;
	pPersonajeNuevo->recursos 	  	= list_create();
	pPersonajeNuevo->valorAlgoritmo = -1;
	pPersonajeNuevo->indiceLlegada = pNivel->cantidadIngresantes++;
	queue_push(pNivel->cListos, pPersonajeNuevo);
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
    bool iEnviarTurno;
    int cantidadListos;

    //Para multiplexar
    int iSocketConexion;
    fd_set readfds;
    FD_ZERO(&readfds);
    tMensaje tipoMensaje;
    tPersonaje* pPersonajeActual;
    char* sPayload;

    bool primerIntento = true;
    pPersonajeActual = NULL;
    iEnviarTurno	 = true; //Lo inicializo en 1 para que de el primer turno

    // Ciclo donde se multiplexa para escuchar personajes que se conectan
    while (1) {

       	wait_personajes(&primerIntento);

       	seleccionarJugador(&pPersonajeActual, pNivel, iEnviarTurno);

        iSocketConexion = multiplexar(&pNivel->masterfds, &readfds, &pNivel->maxSock, &tipoMensaje, &sPayload, logger);

        if (iSocketConexion != -1) {
            pthread_mutex_lock(&semNivel);
            iEnviarTurno = false;
            switch (tipoMensaje) {

            /* Mensajes que puede mandar el personaje */
            case(P_POS_RECURSO):
                posicionRecursoPersonaje(pNivel, pPersonajeActual, iSocketConexion, sPayload, pNivel->socket, logger);
            	iEnviarTurno = coordinarAccesoMultiplesPersonajes(pPersonajeActual, iSocketConexion, true);
                break;

            case(P_MOVIMIENTO):
                movimientoPersonaje(iSocketConexion, sPayload, pNivel, pPersonajeActual, logger);
            	iEnviarTurno = coordinarAccesoMultiplesPersonajes(pPersonajeActual, iSocketConexion, false);
                break;

            case(P_SOLICITUD_RECURSO):
                solicitudRecursoPersonaje(iSocketConexion, sPayload, pNivel, &pPersonajeActual, logger);
            	cantidadListos = queue_size(pNivel->cListos);
            	if (cantidadListos == 0 || cantidadListos == 1) {
					iEnviarTurno = false; //AL reves que N_ENTREGA_RECURSO
				} else if(cantidadListos > 1) {
					iEnviarTurno = true;
				}
                break;

            case(P_DESCONECTARSE_FINALIZADO):
				//TODO personaje  al final del while tiene q mandarme este mensaje cuando termina
				desconectar(pNivel, &pPersonajeActual, iSocketConexion, sPayload);
            	iEnviarTurno = false;
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
             	cantidadListos = queue_size(pNivel->cListos);
            	if (cantidadListos == 0 || cantidadListos == 1) {
					iEnviarTurno = true;
				} else if(cantidadListos > 1) {
					iEnviarTurno = false;
				}
            	break;

            case(N_CONFIRMACION_MOV):
				confirmarMovimiento(pNivel, pPersonajeActual);
            	iEnviarTurno = true;
				break;

            case(N_PERSONAJES_DEADLOCK):
            	matarPersonajeDeadlockeado(pNivel, sPayload);
            	break;

//            case(DESCONEXION): //TODO el personaje tiene que mandarte un P_DESCONECTARSE_FINALIZADO cuando termina sus objetivos
//            	log_debug(logger, "<<< %s", enumToString(DESCONEXION));
//				desconectar(pNivel, &pPersonajeActual, iSocketConexion, sPayload);
//            	iEnviarTurno = false;
//				break;

            default:
            	log_error(logger, "ERROR: estoy en el case por default");
                break;
            } //Fin de switch

            pthread_mutex_unlock(&semNivel);

        } //Fin del if

    } //Fin del while

    pthread_exit(NULL);
}


int seleccionarJugador(tPersonaje** pPersonaje, tNivel* nivel, bool iEnviarTurno) {
    int iTamanioColaListos, iTamanioListaBlock;

    // Me fijo si puede seguir jugando
    if (*pPersonaje != NULL) {
        switch(nivel->algoritmo) {
        case RR:
            if ((*pPersonaje)->valorAlgoritmo < nivel->quantum) {
            	log_debug(logger, "RR: Planificando....");
                // Puede seguir jugando
            	if (iEnviarTurno == true) {
            		enviarTurno(*pPersonaje, nivel->delay);
            	}
                return (EXIT_SUCCESS);
            } else {
                // Termina su quantum vuelve a la cola
                (*pPersonaje)->valorAlgoritmo = 0;
            	queue_push(nivel->cListos, *pPersonaje);
            }
            break;

        case SRDF:
            if ((*pPersonaje)->valorAlgoritmo > 0) {
            	log_debug(logger, "SRDF: Planificando....");
                // Puede seguir jugando
            	if (iEnviarTurno == true) {
					enviarTurno(*pPersonaje, nivel->delay);
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
    iTamanioColaListos = queue_size(nivel->cListos);
    iTamanioListaBlock = list_size(nivel->lBloqueados);

    if (iTamanioColaListos == 0 && iTamanioListaBlock == 0 && pPersonaje == NULL) {
    	log_debug(logger, "No quedan mas personajes en el nivel. Planificador finalizado");
    	return EXIT_SUCCESS;
    } else if (iTamanioColaListos == 1) {
    	//Si estan aqui es porque su quantum terminó
        *pPersonaje = queue_pop(nivel->cListos);
        switch(nivel->algoritmo) {
		case RR:
			break;
		case SRDF:
			//Cuando hay 2 personajes, y uno llega al recurso, se bloquea y luego entra a este case
			break;
		}

    } else if (iTamanioColaListos > 1) {
        switch(nivel->algoritmo) {
        case RR:
        	log_debug(logger, "Planificando RR...");
            *pPersonaje = queue_pop(nivel->cListos);
            (*pPersonaje)->valorAlgoritmo = 0;
            iEnviarTurno = true;
            break;
        case SRDF:
        	//Si esta aqui tiene que haber 3 o mas personajes
        	log_debug(logger, "Planificando SRDF...");
            *pPersonaje = planificacionSRDF(nivel->cListos);
            break;
        }
    }

    if(iEnviarTurno == true){
    	enviarTurno(*pPersonaje, nivel->delay);
    }

    return EXIT_SUCCESS;
}

void enviarTurno(tPersonaje *pPersonaje, int delay) {
	tPaquete pkgProximoTurno;
	pkgProximoTurno.type = PL_OTORGA_TURNO;
	pkgProximoTurno.length = 0;
	usleep(delay);
	char *msjInfo = malloc(sizeof(char) * 100);
	sprintf(msjInfo, "Turno al personaje %c", pPersonaje->simbolo);
	enviarPaquete(pPersonaje->socket, &pkgProximoTurno, logger, msjInfo);
	free(msjInfo);
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

tPersonaje* planificacionSRDF(t_queue* cListos) {
    int iNroPersonaje;
    int iDistanciaFaltanteMinima = 4000000;
    tPersonaje *pPersonaje, *pPersonajeTemp;
    log_debug(logger, "Planificadoooooooooooooooooooooooooooor SRDF");
    int iTamanioCola = queue_size(cListos);
    for (iNroPersonaje = 0; iNroPersonaje < iTamanioCola; iNroPersonaje ++) {
        pPersonajeTemp = list_get(cListos->elements, iNroPersonaje);
        if ((pPersonajeTemp->valorAlgoritmo > 0) && (pPersonajeTemp->valorAlgoritmo < iDistanciaFaltanteMinima)) {
            iDistanciaFaltanteMinima = pPersonajeTemp->valorAlgoritmo;
            pPersonaje = pPersonajeTemp;
        }
    }

    if (iDistanciaFaltanteMinima == 4000000) { // Si no se encontro ninguno que cumpla los requisitos, agarro el primero de la cola
        pPersonaje = queue_pop(cListos);
        pPersonaje->valorAlgoritmo = -1; // -1 quiere decir que no tiene data
    }

    return pPersonaje;
}

void posicionRecursoPersonaje(tNivel *pNivel, tPersonaje*pPersonajeActual, int iSocketConexion, char* sPayload, int socketNivel, t_log* logger) {
    tMensaje tipoMensaje;
    tPaquete pkgPosRecurso;
    pkgPosRecurso.type   = PL_POS_RECURSO;
    pkgPosRecurso.length = sizeof(tSimbolo) + sizeof(tSimbolo);
    memcpy(pkgPosRecurso.payload, sPayload, pkgPosRecurso.length);

    enviarPaquete(socketNivel, &pkgPosRecurso, logger, "Solicitud al NIVEL la posicion de recurso");
    recibirPaquete(socketNivel, &tipoMensaje, &sPayload, logger, "Recibo del NIVEL posicion del recurso");

    if (tipoMensaje == N_POS_RECURSO) {
    	//Este es un nuevo mensaje que me manda el nivel con la posicion del recurso y la distancia del personaje al recurso tmb
    	//TODO esto es para el SRDF
//       	tRtaPosicion2 *rtaPosicion = deserializarRtaPosicion2(sPayload);
//    	if(pPersonajeActual->socket == iSocketConexion){
//    		pPersonajeActual->valorAlgoritmo = rtaPosicion->RD;
//    	}
//    	else {
//    		tPersonaje *personajeEntrante = getPersonaje(pNivel->cListos->elements, iSocketConexion, bySocket);
//    		personajeEntrante->valorAlgoritmo = rtaPosicion->RD;
//    	}
//    	tRtaPosicion rtaPosPersonaje;
//    	rtaPosPersonaje.posX = rtaPosicion->posX;
//    	rtaPosPersonaje.posY = rtaPosicion->posY;
//    	serializarRtaPosicion(PL_POS_RECURSO, rtaPosPersonaje, &pkgPosRecurso);

    	pkgPosRecurso.type    = PL_POS_RECURSO;
        pkgPosRecurso.length  = sizeof(int8_t) + sizeof(int8_t);
        memcpy(pkgPosRecurso.payload, sPayload, pkgPosRecurso.length);
    	enviarPaquete(iSocketConexion, &pkgPosRecurso, logger, "Envio de posicion de recurso al personaje");
    } else {
        pkgPosRecurso.type    = NO_SE_OBTIENE_RESPUESTA;
        pkgPosRecurso.length  = 0;
        enviarPaquete(iSocketConexion, &pkgPosRecurso, logger, "WARN: No se obtiene respuesta esperada del nivel");
    }

    free(sPayload);
}

void muertePorEnemigoPersonaje(char* sPayload, tNivel *pNivel, tPersonaje* pPersonaje, t_log* logger) {
	tPaquete pkgMuertePers;
	pkgMuertePers.type    = PL_MUERTO_POR_ENEMIGO;
	pkgMuertePers.length  = strlen(sPayload);

	/* TODO analizar bien que pasa cuando muere el personaje */

	enviarPaquete(pPersonaje->socket, &pkgMuertePers, logger, "Envio mensaje de muerte por personaje");
}

void matarPersonajeDeadlockeado(tNivel *pNivel, char *sPayload){
	//El nivel me manda en el payload todos los personajes matados
	char *personajesDeadlock = sPayload;
	int cantidadPersonajes = strlen(personajesDeadlock);
	int indicePersonaje;
	tPersonaje *personajeVictima;
	tPaquete paquete;
	int indiceLlegada = 99;

	log_debug(logger, "Me llego mensaje de deadlock del %s", pNivel->nombre);

	for(indicePersonaje=0; indicePersonaje < cantidadPersonajes; indicePersonaje++){
		tSimbolo pSimboloPers = personajesDeadlock[ indicePersonaje ];
		tPersonajeBloqueado *persBlock = getPersonajeBlock(pNivel->lBloqueados, pSimboloPers, bySymbol);

		//Me quedo con el que tenga menor indice, ese entró primero que los demas
		if(indiceLlegada > persBlock->pPersonaje->indiceLlegada){
			//Seteo una nueva victima
			personajeVictima = (persBlock->pPersonaje);
		}
	}

	log_debug(logger, "\n>>>>>VICTIMA: %c\n", personajeVictima->simbolo);
	paquete.length = 0;
	paquete.type = PL_MUERTO_POR_DEADLOCK; //Cuando el personaje se desconecte va a liberar recursos ahí.
	enviarPaquete(personajeVictima->socket, &paquete, logger, "Se mato a un personaje por deadlock");

	free(sPayload);
}

void movimientoPersonaje(int iSocketConexion, char* sPayload, tNivel *pNivel, tPersonaje* pPersonaje, t_log* logger) {
    tPaquete pkgMovimientoPers;
    tMovimientoPers *movPers = deserializarMovimientoPers(sPayload);
    serializarMovimientoPers(PL_MOV_PERSONAJE, *movPers, &pkgMovimientoPers);

    enviarPaquete(pNivel->socket, &pkgMovimientoPers, logger, "Envio movimiento del personaje");

	free(sPayload);
	free(movPers);
}

void confirmarMovimiento(tNivel *nivel, tPersonaje *pPersonajeActual) {
	int sockPersonaje = pPersonajeActual->socket;

	if (nivel->algoritmo == RR) {
		pPersonajeActual->valorAlgoritmo++;
	} else {
		pPersonajeActual->valorAlgoritmo--; //TODO
	}
	tPaquete pkgConfirmacionMov;
	pkgConfirmacionMov.type    = PL_CONFIRMACION_MOV;
	pkgConfirmacionMov.length  = 0;
	log_trace(logger, "Personaje %c con socket %d >>> Confirmacion movimiento", pPersonajeActual->simbolo, pPersonajeActual->socket);
	enviarPaquete(sockPersonaje, &pkgConfirmacionMov, logger, "Se envia confirmacion de movimiento al personaje");
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

void solicitudRecursoPersonaje(int iSocketConexion, char *sPayload, tNivel *pNivel, tPersonaje **pPersonaje, t_log *logger) {
    tPaquete pkgSolicituRecurso;
    pkgSolicituRecurso.type    = PL_SOLICITUD_RECURSO;
    pkgSolicituRecurso.length  = sizeof(tSimbolo) + sizeof(tSimbolo);
    memcpy(pkgSolicituRecurso.payload, sPayload, pkgSolicituRecurso.length);

    tSimbolo *recurso;
    recurso = deserializarSimbolo(sPayload);
    log_info(logger, "El personaje %c solicita el recurso %c", (*pPersonaje)->simbolo, *recurso);

    tPersonajeBloqueado *pPersonajeBloqueado =  createPersonajeBlock(*pPersonaje, *recurso);
    list_add(pNivel->lBloqueados, pPersonajeBloqueado);
    log_debug(logger, "Personaje %c se encuentra bloqueado por recurso %c", pPersonajeBloqueado->pPersonaje->simbolo, *recurso);

    *pPersonaje = NULL;
    free(sPayload);
    free(recurso);

    enviarPaquete(pNivel->socket, &pkgSolicituRecurso, logger, "Solicitud de recurso");
}

void recepcionRecurso(tNivel *pNivel, char *sPayload, t_log *logger) {
	tSimbolo *pSimbolo;
	tPersonaje *pPersonaje;

	pSimbolo = deserializarSimbolo(sPayload);

	pPersonaje = desbloquearPersonaje(pNivel->lBloqueados, *pSimbolo);
	log_info(logger, " <<< Se recibe recurso solicitado por el personaje %c", pPersonaje->simbolo);

	if (pPersonaje != NULL) {
		log_info(logger, "Se desbloquea el personaje: %c", pPersonaje->simbolo);
	    //Aqui actualizo su quantum
		if (pNivel->algoritmo == RR) {
	    	pPersonaje->valorAlgoritmo = pNivel->quantum;
	    } else {
	    	pPersonaje->valorAlgoritmo = 0;
	    }
		queue_push(pNivel->cListos, pPersonaje);
		log_info(logger, "Tamaño de listos: %d", queue_size(pNivel->cListos));
	} else {
		log_info(logger, "No se encontro ningun personaje esperando por el recurso");
		sleep(8000); //Le pongo un sleep y no un exit para que se pueda ver el estado de cada proceso bien clarito
	}

	tPaquete pkgRecursoOtorgado;
	pkgRecursoOtorgado.type = PL_RECURSO_OTORGADO;
	pkgRecursoOtorgado.length = 0;
	enviarPaquete(pPersonaje->socket, &pkgRecursoOtorgado, logger, "Confirmo otorgamiento de recurso al personaje");

	free(pSimbolo);
	free(sPayload);

}

int desconectar(tNivel *pNivel, tPersonaje **pPersonajeActual, int iSocketConexion, char *sPayload) {
	tPersonaje *pPersonaje;
	log_info(logger, "Se detecta desconexion...");
	/*UNDER CONSTRUCTION*/

	if (iSocketConexion == (*pPersonajeActual)->socket) {
		log_info(logger, "Se desconecto el personaje %c", (*pPersonajeActual)->simbolo);
		liberarRecursos(*pPersonajeActual, pNivel);
		tPaquete pkgDesconexionPers;
		serializarSimbolo(PL_DESCONEXION_PERSONAJE, (*pPersonajeActual)->simbolo, &pkgDesconexionPers);
		enviarPaquete(pNivel->socket, &pkgDesconexionPers, logger, "Se envia desconexion del personaje actual al nivel");
		free(*pPersonajeActual);
		*pPersonajeActual = NULL;
		delegarConexion(&setSocketsOrquestador, &pNivel->masterfds, iSocketConexion, &iSocketMaximoOrquestador);
		return EXIT_SUCCESS;
	}

	pPersonaje = sacarPersonajeDeListas(pNivel, iSocketConexion);
	if (pPersonaje != NULL) {
		liberarRecursos(pPersonaje, pNivel); //TODO armar un mensaje para pasarle esto al nivel
		log_info(logger, "Se desconecto el personaje %c", pPersonaje->simbolo);
		tPaquete pkgDesconexionPers;
		serializarSimbolo(PL_DESCONEXION_PERSONAJE, pPersonaje->simbolo, &pkgDesconexionPers);
		enviarPaquete(pNivel->socket, &pkgDesconexionPers, logger, "Se envia desconexion del personaje al nivel");
		free(pPersonaje);
		delegarConexion(&setSocketsOrquestador, &pNivel->masterfds, iSocketConexion, &iSocketMaximoOrquestador);
		return EXIT_SUCCESS;
	} else {
		log_error(logger, "ERROR: no se encontró el personaje que salio en socket %d", iSocketConexion);
		return EXIT_FAILURE;
	}

	return EXIT_FAILURE;
}

/*
 * Se liberan los recursos que poseia el personaje en el nivel y en caso de que un personaje estaba bloqueado por uno de estos, se libera
 * */
char *liberarRecursos(tPersonaje *pPersMuerto, tNivel *pNivel) {
	int iCantidadBloqueados = list_size(pNivel->lBloqueados);
	int iCantidadRecursos	= list_size(pPersMuerto->recursos);

	if ((iCantidadBloqueados == 0) || (iCantidadRecursos == 0)) {
		/* No hay nada que liberar */
		log_debug(logger, "Personaje %c -> Cantidad de recursos asignados = %d", pPersMuerto->simbolo, list_size(pPersMuerto->recursos));
		log_info(logger, "No se reasignarán recursos con la muerte de %c", pPersMuerto->simbolo);
		return getRecursosNoAsignados(pPersMuerto->recursos);
	} else {

		int iIndexBloqueados, iIndexRecursos;
		tPersonajeBloqueado *pPersonajeBloqueado;
		tPersonaje *pPersonajeLiberado;
		tSimbolo *pRecurso;

		/* Respetando el orden en que se bloquearon voy viendo si se libero el recurso que esperaban */
		for (iIndexBloqueados = 0; iIndexBloqueados < iCantidadBloqueados; iIndexBloqueados++) {
			pPersonajeBloqueado = (tPersonajeBloqueado *)list_get(pNivel->lBloqueados, iIndexBloqueados);

			for (iIndexRecursos = 0; iIndexRecursos < iCantidadRecursos; iIndexRecursos++) {
				pRecurso = (tSimbolo *)list_get(pPersMuerto->recursos, iIndexRecursos);

				if (pPersonajeBloqueado->recursoEsperado == *pRecurso) {
					pPersonajeBloqueado = (tPersonajeBloqueado *)list_remove(pNivel->lBloqueados, iIndexBloqueados);
					/* Como saco un personaje de la lista, actualizo la iteracion de los bloqueados */
					iIndexBloqueados--;
					iCantidadBloqueados--;
					pPersonajeLiberado = pPersonajeBloqueado->pPersonaje;
					free(pPersonajeBloqueado);

					pRecurso = (tSimbolo *)list_remove(pPersMuerto->recursos, iIndexRecursos);
					/* Como saco un recurso de la lista, actualizo la iteracion de los recursos */
					iIndexRecursos--;
					iCantidadRecursos--;

					log_info(logger, "Por la muerte de %c se desbloquea %c que estaba esperando por el recurso %c",
							pPersMuerto->simbolo, pPersonajeLiberado->simbolo, *pRecurso);

					list_add(pPersonajeLiberado->recursos, pRecurso);
					queue_push(pNivel->cListos, pPersonajeLiberado);
				}
			}
		}
		return getRecursosNoAsignados(pPersMuerto->recursos);
	}
}

/*
 * Recibe una lista de recursos de un personaje muerto y retorna un puntero a string con esos recursos; y libera memoria de la lista
 */
char *getRecursosNoAsignados(t_list *recursos){

	int cantRecursosNoAsignados = list_size(recursos);
	int i;
	tSimbolo *pRecurso;
	if(cantRecursosNoAsignados != 0){
		//Aloco memoria en un string para mandarle al nivel con los recursos no asignados a nadie
		char *recursosNoAsignados = malloc(cantRecursosNoAsignados*sizeof(char));

		for(i = 0; i < cantRecursosNoAsignados; i++){
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
				return iNivelLoop;
			}
		}
	}

	return -1;
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

tPersonaje *getPersonaje(t_list *listaPersonajes, int valor, tBusquedaPersonaje criterio){
	int iPersonajeLoop;
	int iCantPersonajes = list_size(listaPersonajes);
	tPersonaje* pPersonajeGuardado;

	if (iCantPersonajes > 0) {
		for (iPersonajeLoop = 0; iPersonajeLoop < iCantPersonajes; iPersonajeLoop++) {
			pPersonajeGuardado = (tPersonaje *)list_get(listaPersonajes, iPersonajeLoop);
			switch(criterio){
			case bySocket:
				if(pPersonajeGuardado->socket == valor)
					return pPersonajeGuardado;
				break;
			case byName:
				if(pPersonajeGuardado->simbolo == (tSimbolo)valor)
					return pPersonajeGuardado;
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
			switch(criterio){
			case bySock:
				if(pPersonajeGuardado->pPersonaje->socket == valor)
					return pPersonajeGuardado;
				break;
			case bySymbol:
				if(pPersonajeGuardado->pPersonaje->simbolo == (tSimbolo)valor)
					return pPersonajeGuardado;
				break;
			case byRecursoBlock:
				if(pPersonajeGuardado->recursoEsperado == (tSimbolo)valor)
					return pPersonajeGuardado;
				break;
			}
		}
	}
	return NULL;
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
		switch(criterio){
		case bySock:
			if(pPersonajeGuardado->pPersonaje->socket == valor)
				return i;
			break;
		case bySymbol:
			if(pPersonajeGuardado->pPersonaje->simbolo == (tSimbolo)valor)
				return i;
			break;
		case byRecursoBlock:
			if(pPersonajeGuardado->recursoEsperado == (tSimbolo)valor)
				return i;
			break;
		}
	}
	return -1;
}

/*
 * Elimina al personaje que contiene el socket pasado por parametro de todas las listas del nivel y lo devuelve, si no lo encuentra devuelve null
 */
tPersonaje *sacarPersonajeDeListas(tNivel *pNivel, int iSocket) {
	int iIndicePersonaje;

	iIndicePersonaje = existePersonaje(pNivel->cListos->elements, iSocket, bySocket);
	log_debug(logger, "iIndicePersonaje = %d", iIndicePersonaje);

	if (iIndicePersonaje != -1) {

		return (list_remove(pNivel->cListos->elements, iIndicePersonaje)); // Y sacamos al personaje de la lista de bloqueados
	}

	iIndicePersonaje = existePersonaje(pNivel->lBloqueados, iSocket, bySocket);

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
	tPersonajeBloqueado * pPersonajeBlock;

	if (queue_is_empty(pNivel->cListos) && (pPersonaje == NULL)) {
		sprintf(retorno, "Lista de: %s\n\tEjecutando:\n\tListos: \t", pNivel->nombre);
	} else {
		if(pPersonaje != NULL){
			sprintf(retorno, "Lista de: %s\n\tEjecutando: %c\n\tListos: \t", pNivel->nombre, pPersonaje->simbolo);
		}
		else{
			sprintf(retorno, "Lista de: %s\n\tEjecutando:\n\tListos: \t", pNivel->nombre);
		}
	}

	int iCantidadListos = queue_size(pNivel->cListos);
	for (i = 0; i < iCantidadListos; i++) {
		pPersAux = (tPersonaje *)list_get(pNivel->cListos->elements, i);
		sprintf(tmp, "%c -> ", pPersAux->simbolo);
		string_append(&retorno, tmp);
	}

	sprintf(tmp, "\n\tBloqueados: \t");
	string_append(&retorno, tmp);

	int iCantidadBloqueados = list_size(pNivel->lBloqueados);
	for (i = 0; i < iCantidadBloqueados; i++) {
		pPersonajeBlock = (tPersonajeBloqueado *) list_get(pNivel->lBloqueados, i);
		sprintf(tmp, "%c -> ", pPersonajeBlock->pPersonaje->simbolo);
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
	int iIndicePersonaje;

	iIndicePersonaje = existPersonajeBlock(lBloqueados, recurso, byRecursoBlock);

	if (iIndicePersonaje != -1) {
		tPersonaje *personaje = removePersonajeOfBlock(lBloqueados, iIndicePersonaje);
		if(personaje == NULL){
			log_debug(logger, "personaje es NULL");
			sleep(40);
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
tPersonaje *removePersonajeOfBlock(t_list *block, int indicePersonaje){
	tPersonaje *personaje;
	tPersonajeBloqueado *personajeBlock;
	personajeBlock = list_remove(block, indicePersonaje);
	personaje = personajeBlock->pPersonaje;
	free(personajeBlock);
	return personaje;
}

