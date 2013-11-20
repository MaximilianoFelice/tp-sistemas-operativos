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

void *orquestador(unsigned short usPuerto) {
	t_list *lPlanificadores;

	// Creo la lista de niveles y planificadores
	lPlanificadores = list_create();
	listaNiveles    = list_create();

	// Definicion de variables para los sockets
	fd_set socketsOrquestador;
	int maxSock;
	int socketEscucha;
	int iSocketComunicacion; // Aqui va a recibir el numero de socket que retornar getSockChanged()

	// Inicializacion de sockets y actualizacion del log
	socketEscucha = crearSocketEscucha(usPuerto, logger);
	maxSock = socketEscucha;

	tMensaje tipoMensaje;
	char * sPayload;

	while (1) {
		iSocketComunicacion = getConnection(&socketsOrquestador, &maxSock, socketEscucha, &tipoMensaje, &sPayload, logger, "Orquestador");

		if (iSocketComunicacion != -1) {
			int iCantidadNiveles;
			pthread_mutex_lock(&semNivel);
			
			switch (tipoMensaje) {
			case N_HANDSHAKE: // Un nuevo nivel se conecta
				conexionNivel(iSocketComunicacion, sPayload, socketsOrquestador, lPlanificadores);
				break;

			case P_HANDSHAKE:
				conexionPersonaje(iSocketComunicacion, &socketsOrquestador, sPayload);
				break;
			}

			pthread_mutex_unlock(&semNivel);

		}//Fin del if

	}//Fin del while

	return NULL;
}

void conexionNivel(int iSocketComunicacion, char* sPayload, fd_set* socketsOrquestador, t_list *lPlanificadores) {
	if (!list_is_empty(listaNiveles)) {
		tNivel *pNivelGuardado;
		int bEncontrado  = 0;
		int iNivelLoop   = 0;
		int iCantNiveles = list_size(listaNiveles);

		for (iNivelLoop = 0; (iNivelLoop < iCantNiveles) && (bEncontrado == 0); iNivelLoop++) {
			pNivelGuardado = (tNivel *)list_get(listaNiveles, iNivelLoop);
			bEncontrado    = (strcmp (pNivelGuardado->nombre, sPayload) == 0);
		}

		if (bEncontrado == 1) { //TODO avisar a los q hacen pesonaje que traten este mensaje
			tPaquete pkgNivelRepetido;
			pkgNivelRepetido.type   = PL_NIVEL_YA_EXISTENTE;
			pkgNivelRepetido.length = 0;
			enviarPaquete(iSocketComunicacion, &pkgNivelRepetido, logger, "Ya se encuentra conectado al orquestador un nivel con el mismo nombre");
			break;
		}
	}

	pthread_t *pPlanificador;
	int tipoMensaje;

	tNivel *nivelNuevo = (tNivel *) malloc(sizeof(tNivel));
	pPlanificador 	   = (pthread_t *) malloc(sizeof(pthread_t));
	log_debug(logger, "Se conecto el nivel %s", sPayload);
	char * sNombreNivel = strdup(sPayload);

	tPaquete pkgHandshake;
	pkgHandshake.type   = PL_HANDSHAKE;
	pkgHandshake.length = 0;
	enviarPaquete(iSocketComunicacion, &pkgHandshake, logger, "Handshake Plataforma");

	// Ahora debo esperar a que me llegue la informacion de planificacion.
	recibirPaquete(iSocketComunicacion, &tipoMensaje, &sPayload, logger, "Recibe mensaje informacion del nivel");
	tInfoNivel* pInfoNivel;
	pInfoNivel = deserializarInfoeNivel(sPayload);

	// Validacion de que el nivel me envia informacion correcta
	if (tipoMensaje == N_DATOS) {

		crearNivel(listaNiveles, nivelNuevo, iSocketComunicacion, sNombreNivel, pInfoNivel.algoritmo, pInfoNivel.quantum, pInfoNivel.delay);

		crearHiloPlanificador(pPlanificador, nivelNuevo, lPlanificadores);

		inicializarConexion(&nivelNuevo->masterfds, &nivelNuevo->maxSock, &iSocketComunicacion);

		delegarConexion(&nivelNuevo->masterfds, &socketsOrquestador, &iSocketComunicacion, &nivelNuevo->maxSock);

		// Logueo el nuevo hilo recien creado
		log_debug(logger, "Nuevo planificador del nivel: '%s' y planifica con: %s", nivelNuevo->nombre, nivelNuevo->algoritmo);


	} else {
		log_error(logger,"Tipo de mensaje incorrecto: se esperaba datos del nivel");
		exit(EXIT_FAILURE);
	}

	free(pInfoNivel);
}

void conexionPersonaje(int iSocketComunicacion, fd_set* socketsOrquestador, char* sPayload) {
	int iCantidadNiveles;

	iCantidadNiveles = list_size(listaNiveles);

	if (iCantidadNiveles == 0) {
		tPaquete pkgNivelInexistente;
		pkgNivelInexistente.type   = PL_NIVEL_INEXISTENTE;
		pkgNivelInexistente.length = 0;
		enviarPaquete(iSocketComunicacion, &pkgNivelInexistente, logger, "No hay niveles conectados a la plataforma");
		return;
	}

	tHandshakePers* pHandshakePers;
	pHandshakePers = deserializarHandshakePersonaje(sPayload);

	tNivel * pNivelPedido = search_nivel_by_name_with_return(pHandshakePers->nombreNivel);

	if (existeNivel(pNivelPedido)) {

		// Logueo del pedido de nivel del personaje
		log_trace(logger, "Se conectó el personaje %c. Pide nivel: %s", pHandshakePers->simbolo, pHandshakePers->nombreNivel);

		tPaquete pkgHandshake;
		pkgHandshake.type   = PL_HANDSHAKE;
		pkgHandshake.length = 0;
		delegarConexion(&pNivelPedido->masterfds, &socketsOrquestador, &iSocketComunicacion, &pNivelPedido->maxSock);

		agregarPersonaje(&pNivelPedido->cListos, pHandshakePers->simbolo, iSocketComunicacion);
		signal_personajes(&pNivelPedido->hay_personajes);

		// Le contesto el handshake
		enviarPaquete(iSocketComunicacion, &pkgHandshake, logger, "Handshake de la plataforma al personaje");

	} else {
		// El nivel solicitado no se encuentra conectado todavia
		tPaquete pkgNivelInexistente;
		pkgNivelInexistente.type   = PL_NIVEL_INEXISTENTE;
		pkgNivelInexistente.length = 0;
		enviarPaquete(iSocketComunicacion, &pkgNivelInexistente, logger, "No se encontro el nivel pedido");
	}

	free(pHandshakePers);
}

void crearNivel(t_list* lNiveles, tNivel* nivelNuevo, int socket, char *levelName, tAlgoritmo algoritmo, int quantum, int delay) {
	nivelNuevo->nombre = malloc(strlen(levelName) + 1);
	strcpy(nivelNuevo->nombre, levelName);
	nivelNuevo->cListos 	= list_create();
	nivelNuevo->cBloqueados = list_create();
	nivelNuevo->cMuertos 	= list_create();
	nivelNuevo->socket 		= socket;
	nivelNuevo->quantum 	= quantum;
	nivelNuevo->algoritmo 	= algoritmo;
	nivelNuevo->delay 		= delay;
	nivelNuevo->maxSock 	= 0;

	list_add(lNiveles, nivelNuevo);
}


void agregarPersonaje(t_queue *cPersonajes, char simbolo, int socket) {
	tPersonaje *pPersonajeNuevo;
	pPersonajeNuevo = malloc(sizeof(tPersonaje));

	pPersonajeNuevo->simbolo  	  = simbolo;
	pPersonajeNuevo->socket 	  = socket;
	pPersonajeNuevo->recursos 	  = list_create();
	pPersonajeNuevo->valorAlgoritmo = -1;

	queue_push(cPersonajes, pPersonajeNuevo);
}

/*
 * PLANIFICADOR
 */


void* planificador(void *vNivel) {

	// Armo el nivel que planifico con los parametros que recibe el hilo
	tNivel *pNivel;
	pNivel = (tNivel*) vNivel;

	//Para multiplexar
	int iSocketConexion;
	fd_set readfds;
	FD_ZERO(&readfds);
	tMensaje tipoMensaje;
	tPersonaje pPersonajeActual = NULL;
	char* sPayload;

	// Ciclo donde se multiplexa para escuchar personajes que se conectan
	while (1) {

		wait_personajes(&pNivel->hay_personajes);
		//si no hay personaje, se saca de la cola y se lo pone a jugar enviandole mensaje para jugar
		seleccionarJugador(pPersonajeActual, pNivel);

		iSocketConexion = multiplexar(&pNivel->masterfds, &readfds, &pNivel->maxSock, &tipoMensaje, sPayload, logger);

		if (iSocketConexion != -1) {

			bool encontrado;
			pthread_mutex_lock(&semNivel);

			switch (tMensaje) {
			case(P_MOVIMIENTO):
				movimientoPersonaje(iSocketConexion, sPayload, pNivel, logger);
				break;

			case(P_POS_RECURSO):
				posicionRecursoPersonaje(iSocketConexion, sPayload);
				break;

			case(P_SIN_VIDAS):
				personajeSinVidas(iSocketConexion, sPayload);
				break;

			case(P_DESCONECTARSE_MUERTE):
				muertePersonaje(iSocketConexion, sPayload);
				break;

			case(P_DESCONECTARSE_FINALIZADO):
				finalizacionPersonaje(iSocketConexion, sPayload);
				break;

			case(N_POS_RECURSO):
				posicionRecursoNivel();
				break;


			default:
				return EXIT_FAILURE;
			}

			pthread_mutex_unlock(&semNivel);

		} //Fin de if(i!=-1)

	return NULL;
}

int seleccionarJugador(tPersonaje* pPersonaje, tNivel* nivel) {

	int iTamanioCola;

	// Me fijo si puede seguir jugando
	if (pPersonaje != NULL) {
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
		pPersonaje 	  = queue_pop(nivel->cListos);

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
	enviarPaquete(pPersonaje.socket, &pkgProximoTurno, logger, "Se otorga turno");

	return EXIT_SUCCESS;
}

tPersonaje* planificacionSRDF(t_queue* cListos, int iTamanioCola) {
	int iNroPersonaje;
	int iSocketPersonaje;
	int iDistanciaFaltanteMinima = 4000;
	tPersonaje* pPersonaje, pPersonajeTemp;

	for (iNroPersonaje = 0; iNroPersonaje < iTamanioCola; iNroPersonaje ++) {
		pPersonajeTemp = list_get(cListos->elements, iNroPersonaje);

		if ((pPersonajeTemp->valorAlgoritmo > 0) && (pPersonajeTemp->valorAlgoritmo < iDistanciaFaltanteMinima)) {
			iDistanciaFaltanteMinima = pPersonajeTemp->valorAlgoritmo;
			pPersonaje = pPersonajeTemp;
		}
	}

	if (iDistanciaFaltanteMinima == 4000) { // Si no se encontro ninguno que cumpla los requisitos, agarro el primero de la cola
		pPersonaje = queue_pop(cListos);
		pPersonaje.valorAlgoritmo = -1; // -1 quiere decir que no tiene data
	}

	return pPersonaje;
}


void movimientoPersonaje(int iSocketConexion, char* sPayload, tNivel *pNivel, t_log* logger) {
	tMensaje tipoMensaje;
	tPaquete pkgMovimientoPers;
	pkgMovimientoPers.type    = PL_MOV_PERSONAJE;
	pkgMovimientoPers.length  = strlen(sPayload);
	pkgMovimientoPers.payload = sPayload;

	// Le envio al nivel el movimiento del personaje
	enviarPaquete(pNivel->socket, &pkgMovimientoPers, logger, "Envio movimiento del personaje");

	recibirPaquete(pNivel->socket, &tipoMensaje, &sPayload, logger, "Recibo estado en el que quedo el personaje");

	if (tipoMensaje == N_MUERTO_POR_ENEMIGO) {

		encontrado = sacarPersonajeDeListas(nivel->l_personajesRdy, nivel->l_personajesBlk, iSocketConexion, pjLevantador);

		desbloquearPersonajes(nivel->cBloqueados, nivel->l_personajesRdy, pjLevantador, encontrado, nivel->nombre, proxPj);

		msj.type = SALIR;
		msj.name = pjLevantador->name;

		enviarPaquete(pNivel->socket, &msj, sizeof(msj), logger,"Salida del nivel");

		recibirPaquete(pNivel->socket, &msj, sizeof(msj), logger,"Recibe confirmacion del nivel");

		//Limpio la lista de recursos que tenia hasta que se murio
		list_clean(pjLevantador->recursos);
		//Lo marco como muerto y lo desmarco cuando pide POSICION_RECURSO
		pjLevantador->kill = true;
		//Lo agrego a la cola de finalizados y lo saco cuando manda msj en el case SALUDO
		list_add(nivel->l_personajesDie, pjLevantador);

		msj.type=MOVIMIENTO;
		msj.detail = MUERTO_ENEMIGOS;
		enviaMensaje(iSocketConexion, &msj, sizeof(msj), logger, "El personaje ha perdido una vida por enemigos");

	}
}


void crearHiloPlanificador(pthread_t *pPlanificador, tNivel *nivelNuevo, t_list *list_p){

	if (pthread_create(pPlanificador, NULL, planificador, (void *)nivelNuevo)) {
		log_error(logger, "pthread_create: %s",strerror(errno));
		exit(EXIT_FAILURE);
	}

	list_add(list_p, pPlanificador);
}



tNivel * search_nivel_by_name_with_return(char *level_name){
	int i;

	for (i = 0; i<list_size(listaNiveles); i++) {
		tNivel* nivel = (tNivel*) list_get(listaNiveles, i);

		if(string_equals_ignore_case(nivel->nombre, level_name))
			return nivel;
	}

	return NULL;
}


tPersonaje *search_pj_by_name_with_return(t_list *lista, char name){
	int contPj;
	tPersonaje *pjLevantador;

	for(contPj =0 ; contPj<list_size(lista); contPj++) {
		pjLevantador = list_get(lista, contPj);

		if(pjLevantador->name == name)
			return pjLevantador;
	}
	return NULL;
}


tPersonaje *search_pj_by_socket_with_return(t_list *lista, int sock){
	tPersonaje * personaje;
	int contPj;
	for(contPj =0 ; contPj<list_size(lista); contPj++){
		personaje = list_get(lista, contPj);
		if(personaje->socket == sock)
			return personaje;
	}
	return NULL;
}

void search_pj_by_socket(t_list *lista, int sock, tPersonaje *personaje){
	int contPj;

	for(contPj =0 ; contPj<list_size(lista); contPj++){
		personaje = list_get(lista, contPj);

		if(personaje->socket == sock)
			break;
	}
}

void search_pj_by_name(t_list *lista, char name, tPersonaje *personaje){
	int contPj;
	for(contPj =0 ; contPj<list_size(lista); contPj++){
		personaje = list_get(lista, contPj);
		if(personaje->name == name)
			break;
	}
	personaje = NULL;
}

int buscarIndicePjPorSocket(t_list *lista, char simbolo, tPersonaje *personaje){
	int contPj;

	for (contPj =0 ; contPj<list_size(lista); contPj++) {
		personaje = list_get(lista, contPj);

		if (personaje->simbolo == simbolo) {
			return contPj;
		}

	}
	return -1;
}

int buscarIndicePjPorSocket(t_list *lista, int sock, tPersonaje *personaje){
	int contPj;

	for(contPj =0 ; contPj<list_size(lista); contPj++){
		personaje = list_get(lista, contPj);

		if(personaje->socket == sock)
			return contPj;
	}
	return -1;
}


void desbloquearPersonajes(t_list* block, t_list *ready, tPersonaje *pjLevantador, bool encontrado, char*nom_nivel, int proxPj){
	int contRec, j;
	tPersonaje* pjLevantadorBlk;
	message_t *msj = malloc(sizeof(message_t));
	char *recurso;

	if (list_size(block) > 0) {
		for (contRec = 0;contRec<list_size(pjLevantador->recursos)-(encontrado ? 1 : 0);contRec++){
			//Levanto un recurso de la lista de recursos del personaje
			recurso = (char *) list_get(pjLevantador->recursos,contRec);

			//Recorro los personajes bloqueados
			for (j = 0;j<list_size(block);j++) {

				//Levanto cada personaje de la lista
				pjLevantadorBlk = list_get(block, j);

				//Si un personaje esta bloqueado, el ultimo recurso de su lista de recursos es el recurso por el cual se bloqueo
				char* recursoLevantado = list_get(pjLevantadorBlk->recursos,list_size(pjLevantadorBlk->recursos) - 1);

				if (*recursoLevantado == *recurso) {
					log_trace(logger,"Se desbloqueo %c por %c recurso",	pjLevantadorBlk->name, *recurso);
					//Desbloquear: lo saco de bloqueados y lo paso a ready
					list_add(ready,pjLevantadorBlk);
					list_remove(block,j);
					//Envio mensaje para desbloquear al chaboncito
					msj->type = MOVIMIENTO;
					log_debug(logger,"Se ha desbloqueado al personaje %c",pjLevantadorBlk->name);
					enviarPaquete(pjLevantadorBlk->socket, &msj, logger,"Se desbloqueo un personaje");

					imprimirLista(nom_nivel,ready,block,proxPj - 1);
					break;
				}
			}
		}
	}

	free(msj);
}

bool exist_personaje(t_list *list, char name_pj, int  *indice_pj) {
	int iIndice, iCantidadPersonajes;
	tPersonaje * pPersonaje;

	if (list_size(list) == 0) {
		return false;
	}

	iCantidadPersonajes = list_size(list);

	for (iIndice=0; iIndice<iCantidadPersonajes; iIndice++) {
		pPersonaje = list_get(list, iIndice);

		if (pPersonaje->name == name_pj) {
			*indice_pj = iIndice;
			return true;
		}
	}
	return false;
}


bool estaMuerto(t_list * end, char name){
	int iIndice, iElementosLista;
	iElementosLista = list_size(end);

	for (iIndice=0; iIndice<iElementosLista; iIndice++) {
		tPersonaje * pjAux = list_get(end, iIndice);

		if (pjAux->name == name) {
			return true;
		}
	}

	return false;
}

bool sacarPersonajeDeListas(t_list *ready, t_list *block, int socket,  tPersonaje *pjLevantador) {

	int indice_personaje;
	bool encontrado = false;

	indice_personaje = search_index_of_pj_by_socket(block, socket, pjLevantador);

	if(indice_personaje != -1){
		encontrado = true; //Si lo encontras en Blk, cambiamos el flag
		pjLevantador = list_remove(block, indice_personaje); // Y sacamos al personaje de la lista de bloqueados
	}
	//Si no lo encuentra: lo busco en la lista de Ready y lo saco de la misma
	if (!encontrado) { //Si no lo encontras, buscarlo en rdy
		indice_personaje = search_index_of_pj_by_socket(ready, socket, pjLevantador);
		pjLevantador = list_remove(ready, indice_personaje); // Lo sacamos de la lista
	}

	return encontrado;
}

void imprimirLista(char* nivel, t_list* rdy, t_list* blk, int cont) {

	char* tmp = malloc(20);
	char* retorno = malloc(500);
	int i;

	tPersonaje* levantador;

	if (list_size(rdy) == 0 || cont < 0) { //Si no hay nadie listo, no se quien esta ejecutando
		sprintf(retorno, "Lista de: %s\n\tEjecutando:\n\tListos: \t", nivel);
	} else {
		levantador = (tPersonaje *)list_get_data(rdy, cont);
		sprintf(retorno, "Lista de: %s\n\tEjecutando: %c\n\tListos: \t", nivel, levantador->simbolo);
	}

	for (i = 0; i < list_size(rdy); i++) {
		levantador = (tPersonaje *)list_get_data(rdy, i);
		sprintf(tmp, "%c -> ", levantador->simbolo);
		string_append(&retorno, tmp);
	}

	sprintf(tmp, "\n\tBloqueados: \t");
	string_append(&retorno, tmp);

	for (i = 0; i < list_size(blk); i++) {
		levantador = list_get(blk, i);
		sprintf(tmp, "%c -> ", levantador->simbolo);
		string_append(&retorno, tmp);
	}

	log_info(logger, retorno);
	free(tmp);
	free(retorno);
}

void delegarConexion(fd_set *master_planif, fd_set *master_orq, int *sock, int *maxSock) {
	//Saco el socket del conjunto de sockets del orquestador
	FD_CLR(*sock, master_orq);
	FD_SET(*sock, master_planif);
	//Lo agrego al conjunto del planificador
	if(FD_ISSET(*sock, master_planif))
		log_debug(logger, "--> Delegue la conexion del personaje al planificador <--");
	else{
		log_warning(logger, "WARN: Error al delegar conexiones");
		exit(EXIT_FAILURE);
	}

	//Actualizo el tope del set de sockets
	if (*sock > *maxSock)
		*maxSock = *sock;
}

void inicializarConexion(fd_set *master_planif, int *maxSock, int *sock) {
	FD_ZERO(master_planif);
	*maxSock = *sock;
}

void imprimirConexiones(fd_set *master_planif, int maxSock, char* host) {
	int i;
	int cantSockets =0;

	log_debug(logger, "Conexiones del %s", host);
	for(i = 0; i<=maxSock; i++){
		if(FD_ISSET(i, master_planif)){
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
	if(!(*hay_personajes)){
		pthread_mutex_lock(&semNivel);
		pthread_cond_wait(&hayPersonajes, &semNivel);
		pthread_mutex_unlock(&semNivel);
	}
}

