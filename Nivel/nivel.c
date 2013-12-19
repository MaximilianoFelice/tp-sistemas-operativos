/*
 * Sistemas Operativos - Super Mario Proc RELOADED.
 * Grupo       : C o no ser.
 * Nombre      : nivel.c.
 * Descripcion : Este archivo contiene la implementacion de las
 * funciones usadas por el nivel.
 */

#include "nivel.h"
#define TAM_EVENTO (sizeof(struct inotify_event)+24)
#define TAM_BUFFER (1024*TAM_EVENTO)//podria ser 1 en lugar de 1024? (ya que es cant.de eventos simultaneos y yo solo pregunto por un evento)

t_log  *logger;
t_list *list_personajes;
t_list *list_items;
bool hayQueAsesinar = true;
bool analizarDeadlock = true;

pthread_mutex_t semItems;


int main(int argc, char** argv) {
	tNivel nivel;
	int iSocket;
	char *configFilePath = argv[1];

	signal(SIGINT, cerrarForzado);


	pthread_mutex_init(&semItems,NULL);

	//LOG
	logger = logInit(argv, "NIVEL");

	/*
	 * FUNCION INIT
	 */
	// INICIALIZANDO GRAFICA DE MAPAS
	list_items 	    = list_create();
	list_personajes = list_create();
	nivel_gui_inicializar();
	nivel_gui_get_area_nivel(&nivel.maxRows, &nivel.maxCols);
	log_debug(logger, "Se va a levantar el archivo de configuracion...");
	levantarArchivoConf(configFilePath, &nivel);

	//SOCKETS
	iSocket = connectToServer(nivel.plataforma.IP, nivel.plataforma.port, logger);
    if (iSocket == EXIT_FAILURE) {
    	cerrarNivel("No se puede conectar con la plataforma");
    }

    nivel.plataforma.socket = iSocket;
    log_debug(logger, "SOCKET %d", nivel.plataforma.socket);
    handshakeConPlataforma(&nivel);

	crearEnemigos(&nivel);

	// LANZANDO EL HILO DETECTOR DE INTERBLOQUEO
	//TODO REVISAR
	pthread_t hiloInterbloqueo;
	pthread_create(&hiloInterbloqueo, NULL, &deteccionInterbloqueo, (void *)&nivel);

	escucharConexiones(&nivel, configFilePath);

	log_destroy(logger);
	return EXIT_SUCCESS;
}

void levantarArchivoConf(char* pathConfigFile, tNivel *pNivel) {

	t_config *configNivel;
	tPosicion posCaja;
	int iCaja = 1;
	bool hayCajas = false;
	char* sLineaCaja;
	char** aCaja;
	char* datosPlataforma;


	if (!file_exists(pathConfigFile)) {
		char* messageLimitErr;
		messageLimitErr = string_from_format("[ERROR] '%s' no encontrado o no disponible para la lectura.\n", pathConfigFile);
		cerrarNivel(messageLimitErr);
	}

	configNivel = config_create(pathConfigFile);

	char* str = strdup("Nombre,Recovery,Enemigos,Sleep_Enemigos,Algoritmo,Quantum,Retardo,TiempoChequeoDeadlock,Plataforma");
    char* token;
    token = strtok(str, ",");

    while (token) {
    	if (!(config_has_property(configNivel, token))) {
    		char* messageLimitErr;
			messageLimitErr = string_from_format("[ERROR] No se encontro '%s' en '%s'\n", token, pathConfigFile);
			cerrarNivel(messageLimitErr);
		}
        token = strtok(NULL,","); //Pasa al siguiente token
    }

    free(str);

	sLineaCaja = string_from_format("Caja%i", iCaja);

	while (config_has_property(configNivel, sLineaCaja)) {
		hayCajas = true;
		aCaja	 = config_get_array_value(configNivel, sLineaCaja);
		free(sLineaCaja);

		posCaja.x = atoi(aCaja[3]);
		posCaja.y = atoi(aCaja[4]);
		if (posCaja.y > pNivel->maxRows || posCaja.x > pNivel->maxCols || posCaja.y < 1 || posCaja.x < 1) {
			char* messageLimitErr;
			messageLimitErr = string_from_format(
				"La caja %d excede los limites de la pantalla. CajaPos=(%d,%d) - Limites=(%d,%d)",
				iCaja, posCaja.x, posCaja.y, pNivel->maxCols, pNivel->maxRows
			);
			cerrarNivel(messageLimitErr);
		}

		// Si la validacion fue exitosa creamos la caja de recursos
		CrearCaja(list_items, *aCaja[1],atoi(aCaja[3]),atoi(aCaja[4]),atoi(aCaja[2]));
		iCaja++;
		sLineaCaja = string_from_format("Caja%i", iCaja);
	};
	free(sLineaCaja);
	free(*aCaja);

	if (!hayCajas) {
		cerrarNivel("No hay cajas disponibles");
	}
	log_info(logger, "1");
	log_info(logger, "Archivo correcto, se procede a levantar los valores");

	pNivel->cantRecursos   = list_size(list_items);

	pNivel->deadlock.recovery  = config_get_int_value(configNivel, "Recovery");
	pNivel->deadlock.checkTime = config_get_int_value(configNivel, "TiempoChequeoDeadlock");
	log_info(logger, "2");
	pNivel->nombre = string_duplicate(config_get_string_value(configNivel,"Nombre"));

	pNivel->cantEnemigos  = config_get_int_value(configNivel, "Enemigos");

	pNivel->sleepEnemigos = config_get_int_value(configNivel, "Sleep_Enemigos");

	char* algoritmoAux;
	algoritmoAux   = config_get_string_value(configNivel, "Algoritmo");

	if (strcmp(algoritmoAux, "RR") == 0) {
		pNivel->plataforma.algPlanif = RR;
		pNivel->plataforma.valorAlgorimo = config_get_int_value(configNivel, "Quantum");
	} else {
		pNivel->plataforma.algPlanif = SRDF;
	}

	pNivel->plataforma.delay = config_get_int_value(configNivel, "Retardo");

	datosPlataforma = config_get_string_value(configNivel, "Plataforma");

	char ** aDatosPlataforma = string_split(datosPlataforma, ":");
	strcpy(pNivel->plataforma.IP, aDatosPlataforma[0]);
	pNivel->plataforma.port  = atoi(aDatosPlataforma[1]);
	free(aDatosPlataforma[0]);
	free(aDatosPlataforma[1]);
	free(aDatosPlataforma);

	config_destroy(configNivel);
}

void handshakeConPlataforma(tNivel *pNivel) {
	tPaquete paquete;
	int largoNombre = strlen(pNivel->nombre) + 1;
	paquete.type   = N_HANDSHAKE;
	paquete.length = largoNombre;
	memcpy(paquete.payload, pNivel->nombre, largoNombre);
	enviarPaquete(pNivel->plataforma.socket, &paquete, logger, "Se envia handshake a plataforma");

	tMensaje tipoDeMensaje;
	char* payload;

	recibirPaquete(pNivel->plataforma.socket,&tipoDeMensaje,&payload,logger,"Se recibe handshake de plataforma");

	if(tipoDeMensaje != PL_HANDSHAKE){
		cerrarNivel("Nos llego mal el mensaje");
	}

	tInfoNivel infoDeNivel;
	tipoDeMensaje=N_DATOS;
	infoDeNivel.algoritmo = pNivel->plataforma.algPlanif;
	infoDeNivel.quantum	  = pNivel->plataforma.valorAlgorimo;
	infoDeNivel.delay	  = pNivel->plataforma.delay;
	serializarInfoNivel(N_DATOS, infoDeNivel, &paquete);

	enviarPaquete(pNivel->plataforma.socket, &paquete, logger, "Se envia a la plataforma los criterios de planificacion");
}

void crearEnemigos(tNivel *nivel) {
	// CREANDO Y LANZANDO HILOS ENEMIGOS
	if (nivel->cantEnemigos > 0) {
		int indexEnemigos;
		tEnemigo *aHilosEnemigos;
		log_debug(logger,"Cantidad de enemigos %d ", nivel->cantEnemigos);
		aHilosEnemigos = calloc(nivel->cantEnemigos, sizeof(tEnemigo));

		for (indexEnemigos = 0; indexEnemigos < nivel->cantEnemigos; indexEnemigos++) {
			aHilosEnemigos[indexEnemigos].ID = indexEnemigos + 1; //El numero o id de enemigo
			aHilosEnemigos[indexEnemigos].pNivel = nivel;

			if (pthread_create(&aHilosEnemigos[indexEnemigos].thread, NULL, enemigo, (void*)&aHilosEnemigos[indexEnemigos] )) {
				log_error(logger, "pthread_create: %s", strerror(errno));
				exit(EXIT_FAILURE);
			}
		}

	} else {
		nivel_gui_dibujar(list_items, nivel->nombre);
	}
}

void escucharConexiones(tNivel *pNivel, char* configFilePath) {
	// Variables del Poll

	//INOTIFY
	int descriptorInotify   = inotify_init();
	int watch_id = inotify_add_watch(descriptorInotify, configFilePath, IN_MODIFY);
	if (watch_id == -1) {
		log_error(logger, "Error en inotify add_watch");
	}

	struct pollfd uDescriptores[POLL_NRO_FDS];
	uDescriptores[0].fd	    = pNivel->plataforma.socket;
	uDescriptores[0].events = POLLIN;
	uDescriptores[1].fd 	= descriptorInotify;
	uDescriptores[1].events = POLLIN;
	int iResultadoPoll;

	char *sPayload;
	tMensaje tipoDeMensaje;
	int iSocket = pNivel->plataforma.socket;

	while (1) {

		if ((iResultadoPoll = poll(uDescriptores, POLL_NRO_FDS, POLL_TIMEOUT)) == -1) {
			log_error(logger, "Error al escuchar en el polling");

		} else {

			if (uDescriptores[1].revents & POLLIN) { // Hay data lista para recibir
				struct inotify_event evento;
				int i = read(descriptorInotify, &evento, sizeof(struct inotify_event));
				if (i <= 0) {
					perror("la cague");
				}
				if (evento.mask & IN_MODIFY) {//avisar a planificador que cambio el archivo config
					log_debug(logger, "DETECTA CAMBIO EN EL INOTIFY");
					actualizarInfoNivel(pNivel, iSocket,configFilePath);
				}
				inotify_rm_watch(descriptorInotify, watch_id);
				close(descriptorInotify);

				descriptorInotify = inotify_init();
				watch_id = inotify_add_watch(descriptorInotify, configFilePath, IN_MODIFY);
				if (watch_id == -1) {
					log_error(logger, "Error en inotify add_watch");
				}
				uDescriptores[1].fd 	= descriptorInotify;
				uDescriptores[1].events = POLLIN;
			}

			//Preguntar por que si poll detecta actividad en el descriptor del inotify y este solo se acciona cuando ocurre in_modify => haria falta todo lo que sigue? o
			 //simplemente podria actualizar los datos y listo?
			if (uDescriptores[0].revents & POLLIN) { // Hay data lista para recibir

				recibirPaquete(iSocket, &tipoDeMensaje, &sPayload, logger,"Recibiendo mensaje de plataforma");

				pthread_mutex_lock(&semItems);
				switch(tipoDeMensaje) {
				case PL_CONEXION_PERS:
					conexionPersonaje(iSocket, sPayload);
					break;

				case PL_POS_RECURSO:
					posicionRecurso(pNivel, iSocket, sPayload);
					break;

				case PL_MOV_PERSONAJE:
					movimientoPersonaje(pNivel, iSocket, sPayload);
					break;

				case PL_SOLICITUD_RECURSO:
					solicitudRecurso(pNivel, iSocket, sPayload);
					break;

				case PL_LIBERA_RECURSOS:
					liberarRecursosPersonajeMuerto(pNivel, sPayload);
					break;

				case PL_DESCONEXION_PERSONAJE:// Un personaje termino o murio y debo liberar instancias de recursos que tenia asignado
					desconexionPersonaje(pNivel, sPayload);
					break;

				default:
					break;
				} //Fin del switch

				nivel_gui_dibujar(list_items, pNivel->nombre);

				pthread_mutex_unlock(&semItems);
			}
		}
	}
}

void conexionPersonaje(int iSocket, char *sPayload) {
	tSimbolo *simbolo = deserializarSimbolo(sPayload);

	crearNuevoPersonaje(*simbolo);
	notificacionAPlataforma(iSocket, N_CONEXION_EXITOSA, "Se notifica a plataforma que el personaje se conecto exitosamente");

	free(simbolo);
	free(sPayload);
}

void movimientoPersonaje(tNivel *pNivel, int iSocket, char *sPayload) {
	tPersonaje *pPersonaje;
	tPosicion posPersonaje;
	tMovimientoPers* movPersonaje;

	movPersonaje = deserializarMovimientoPers(sPayload);

	log_debug(logger, "<<< %s: Solicitud de movimiento del personaje %c.", pNivel->nombre, movPersonaje->simbolo);

	pPersonaje = getPersonajeBySymbol(movPersonaje->simbolo);

	if (pPersonaje != NULL) {

		char symbol=(char) movPersonaje->simbolo;
		getPosPersonaje(list_items,symbol, &posPersonaje.x, &posPersonaje.y);

		if(!pPersonaje->muerto && !hayAlgunEnemigoArriba(pNivel, posPersonaje.x, posPersonaje.y)){

			pPersonaje->bloqueado=false;

			calcularMovimiento(pNivel, movPersonaje->direccion, &posPersonaje.x, &posPersonaje.y);
			log_debug(logger, "Movimiento de %c en (%d, %d)", movPersonaje->simbolo, posPersonaje.x, posPersonaje.y);

			MoverPersonaje(list_items,symbol, posPersonaje.x, posPersonaje.y);
			pPersonaje->posicion.x = posPersonaje.x;
			pPersonaje->posicion.y = posPersonaje.y;
			notificacionAPlataforma(iSocket, N_CONFIRMACION_MOV, "Notificando a plataforma personaje movido correctamente");

		} else {
			log_info(logger, "-> Un enemigo alcanzo al personaje %c <-", movPersonaje->simbolo);
			matarPersonaje(pNivel, &movPersonaje->simbolo, N_MUERTO_POR_ENEMIGO);
			hayQueAsesinar = true;
		}
	}

	free(sPayload);
	free(movPersonaje);

}

void posicionRecurso(tNivel *pNivel, int iSocket, char *sPayload) {
	tPaquete paquete;
	tPregPosicion *posConsultada = deserializarPregPosicion(sPayload);

	log_debug(logger, "<<< %s Personaje %c solicita la posicion del recurso %c", pNivel->nombre, posConsultada->simbolo, posConsultada->recurso);

	bool buscarRecurso(ITEM_NIVEL *item){
		return ((item->item_type == RECURSO_ITEM_TYPE) && (item->id == posConsultada->recurso) );
	}

	ITEM_NIVEL* pRecurso;
	pRecurso = list_find(list_items, (void*)buscarRecurso);

	if (pRecurso != NULL) {
		tRtaPosicion rtaPosicion;
		rtaPosicion.posX = pRecurso->posx;
		rtaPosicion.posY = pRecurso->posy;
		serializarRtaPosicion(N_POS_RECURSO, rtaPosicion, &paquete);

		enviarPaquete(iSocket, &paquete, logger, "Se envia posicion de recurso a la plataforma");

	} else {
		notificacionAPlataforma(iSocket, N_RECURSO_INEXISTENTE, "El recurso solicitado no existe");
	}

	free(posConsultada);
	free(sPayload);
}

void solicitudRecurso(tNivel *pNivel, int iSocket, char *sPayload) {
	tPregPosicion *posConsultada;
	tPaquete paquete;
	posConsultada = deserializarPregPosicion(sPayload);

	log_debug(logger, "<<< %s: Personaje %c solicita una instancia del recurso %c", pNivel->nombre, (char)posConsultada->simbolo, (char)posConsultada->recurso);
	// Calculo la cantidad de instancias
	int cantInstancias = restarInstanciasRecurso(list_items, posConsultada->recurso);

	tPersonaje *pPersonaje;
	pPersonaje = getPersonajeBySymbol(posConsultada->simbolo);

	if (cantInstancias >= 0) { //SE LO OTORGO EL RECURSO PEDIDO

		log_info(logger, "Al personaje %c se le dio el recurso %c", posConsultada->simbolo, posConsultada->recurso);

		//Agrego el recurso a la lista de recursos del personaje y lo desbloquea si estaba bloqueado
		void agregaRecursoYdesboquea(tPersonaje *personaje){
			if (personaje->simbolo == pPersonaje->simbolo) {
				personaje->bloqueado = false;
				list_add_new(personaje->recursos, &(posConsultada->recurso), sizeof(tSimbolo));
			}
		}

		list_iterate(list_personajes,(void*) agregaRecursoYdesboquea);

		tPregPosicion recursoSolicitado;
		recursoSolicitado.simbolo=pPersonaje->simbolo;
		recursoSolicitado.recurso=posConsultada->recurso;

		serializarPregPosicion(N_ENTREGA_RECURSO, recursoSolicitado, &paquete);

		// Envio mensaje donde confirmo la otorgacion del recurso pedido
		enviarPaquete(iSocket, &paquete, logger, "Se envia confirmacion de otorgamiento de recurso a plataforma");

	} else { //ESTA BLOQUEADO

		log_info(logger,"El personaje %c se bloqueo por el recurso %c",posConsultada->simbolo,posConsultada->recurso);
		//Agrego el recurso a la lista de recursos del personaje y lo bloqueo
		void agregaRecursoYbloquea(tPersonaje *personaje) {
			if (personaje->simbolo == pPersonaje->simbolo) {
				personaje->bloqueado = true;
				list_add_new(personaje->recursos,&(posConsultada->recurso),sizeof(tSimbolo));
			}
		}

		list_iterate(list_personajes,(void*)agregaRecursoYbloquea);

		tPregPosicion recursoSolicitado;
		recursoSolicitado.simbolo=pPersonaje->simbolo;
		recursoSolicitado.recurso=posConsultada->recurso;

		serializarPregPosicion(N_BLOQUEADO_RECURSO, recursoSolicitado, &paquete);
		// Envio mensaje donde confirmo la otorgacion del recurso pedido
		enviarPaquete(iSocket, &paquete, logger, "Se envia confirmacion de otorgamiento de recurso a plataforma");
	}

	free(sPayload);
	free(posConsultada);
}

void liberarRecursosPersonajeMuerto(tNivel *pNivel, char *sPayload){
	tDesconexionPers *persDesconectado = deserializarDesconexionPers(sPayload);

	liberarRecursos(pNivel, persDesconectado);
	free(persDesconectado);
	free(sPayload);
	log_info(logger, "Libere recursos exitosamente");

//	tMensaje tipoDeMensaje;
//	recibirPaquete(pNivel->plataforma.socket, &tipoDeMensaje, &sPayload, logger, "Recibo personajes que se desbloquearon");
//	persDesconectado = deserializarDesconexionPers(sPayload);
//
//	desbloquearPersonajes(pNivel, persDesconectado);
//	free(sPayload);
//	free(persDesconectado);

	log_info(logger, "Desbloquee a los personajes y libere recursos exitosamente");

}

void liberarRecursos(tNivel *pNivel, tDesconexionPers *persDesconectado){

	log_info(logger, "%s: Liberando recursos del personaje %c", pNivel->nombre, persDesconectado->simbolo);

	int i;
	for (i=0; i<persDesconectado->lenghtRecursos; i++) {
		int instancias = sumarInstanciasRecurso(list_items, (char) persDesconectado->recursos[i]);
		log_debug(logger, "Libere una instancia del recurso %c. Ahora tiene %d", persDesconectado->recursos[i], instancias);
	}
}

void desbloquearPersonajes(tNivel *pNivel, tDesconexionPers *persDesconectado){

	log_info(logger, "%s: Desbloqueando personajes por liberacion de recursos", pNivel->nombre);

	int i;
	for (i=0; i<persDesconectado->lenghtRecursos; i++) {
		tPersonaje *personaje = getPersonajeBySymbol((tSimbolo)persDesconectado->recursos[i]);
		personaje->bloqueado = false;
		log_debug(logger, "Desbloqueo al personaje %c", personaje->simbolo);
	}

}

void removePersonajeDeListas(tSimbolo simbolo){
	bool buscarPersonaje(tPersonaje* pPersonaje) {
		return (pPersonaje->simbolo == simbolo);
	}

	tPersonaje *personajeOut = list_remove_by_condition(list_personajes,(void*)buscarPersonaje);

	if (personajeOut == NULL) {
		log_debug(logger, "No se encontro el personaje");
	}

	BorrarItem(list_items, simbolo);
	personaje_destroyer(personajeOut);
}

void desconexionPersonaje(tNivel *pNivel, char *sPayload) {

	tDesconexionPers *persDesconectado = deserializarDesconexionPers(sPayload);

	log_info(logger, "<<< El personaje %c se desconecto", persDesconectado->simbolo);

	// Eliminar al personaje de list_personajes
	bool buscarPersonaje(tPersonaje* pPersonaje) {
		return (pPersonaje->simbolo == persDesconectado->simbolo);
	}

	tPersonaje *personajeOut = list_remove_by_condition(list_personajes,(void*)buscarPersonaje);

	if (personajeOut == NULL) {
		log_debug(logger, "No se encontro el personaje");
	}

	BorrarPersonaje(list_items, persDesconectado->simbolo);

	log_debug(logger, "Elimine al personaje del nivel");

	liberarRecursos(pNivel, persDesconectado);
	free(sPayload);
	free(persDesconectado);
	log_info(logger, "Libere recursos exitosamente");

	tMensaje tipoDeMensaje;
	recibirPaquete(pNivel->plataforma.socket, &tipoDeMensaje, &sPayload, logger, "Recibo personajes que se desbloquearon");
	persDesconectado = deserializarDesconexionPers(sPayload);

	desbloquearPersonajes(pNivel, persDesconectado);
	free(sPayload);
	free(persDesconectado);

	log_info(logger, "Desbloquee a los personajes y libere recursos exitosamente");

	personaje_destroyer(personajeOut);
	analizarDeadlock = true;

}


void actualizarInfoNivel(tNivel *pNivel, int iSocket, char* configFilePath) {
//	char* algoritmoAux;
	log_debug(logger, "antes de crear archivo de configuracion %s", configFilePath);

	if (file_exists(configFilePath)) {
		log_debug(logger, "Leyendo archivo de configuracion %s", configFilePath);
		t_config *configNivel = config_create(configFilePath);
		log_debug(logger, "despues de crear archivo de configuracion %s", configFilePath);

		if (!config_has_property(configNivel,"Algoritmo")) {
			log_debug(logger, "No esta la property algoritmo");
		} else {
			char* algoritmoAux = config_get_string_value(configNivel, "Algoritmo");

			if (strcmp(algoritmoAux,"RR") == 0) {
				pNivel->plataforma.algPlanif = RR;
				pNivel->plataforma.valorAlgorimo = config_get_int_value(configNivel, "Quantum");
				pNivel->plataforma.delay = config_get_int_value(configNivel, "Retardo");
			} else {
				pNivel->plataforma.algPlanif = SRDF;
				pNivel->plataforma.valorAlgorimo = 0;
				pNivel->plataforma.delay = config_get_int_value(configNivel, "Retardo");
			}

			pNivel->plataforma.delay = config_get_int_value(configNivel, "Retardo");
			log_debug(logger, "Se produjo un cambio en el archivo configuracion, Algoritmo: %s quantum:%i retardo:%i",
					algoritmoAux, pNivel->plataforma.valorAlgorimo, pNivel->plataforma.delay);

			tPaquete paquete;
			//ENVIANDO A PLATAFORMA NOTIFICACION DE ALGORITMO ASIGNADO
			tInfoNivel infoDeNivel;
			infoDeNivel.algoritmo = pNivel->plataforma.algPlanif;
			infoDeNivel.quantum   = pNivel->plataforma.valorAlgorimo;
			infoDeNivel.delay     = pNivel->plataforma.delay;
			//serializacion propia porque la de protocolo no me andaba bien

			serializarInfoNivel(N_ACTUALIZACION_CRITERIOS, infoDeNivel, &paquete);
//			tInfoNivel* pepe;
//			pepe = deserializarInfoNivel(paquete.payload);
//
//
//			log_debug(logger, "socket NOTIII           %s, %d, %d", pepe->algoritmo, pepe->delay, pepe->quantum);
			enviarPaquete(iSocket, &paquete, logger, "Notificando cambio de algoritmo a plataforma");
		}
		config_destroy(configNivel);
	}else {
		log_debug(logger, "No esta el archivo");
	}
}


void *enemigo(void * args) {

	tEnemigo *enemigo;
	enemigo = (tEnemigo *)args;

	//Variables de movimiento
	tDirMovimiento dirMovimiento = vacio;
	char ultimoMov     = 'a';
	char dirMov	       = 'b';
	//Variables de persecucion de victima
	tPersonaje* persVictima;
	bool estaEnRecurso = false;


	bool esUnRecurso(ITEM_NIVEL *itemNiv){
		return (itemNiv->item_type==RECURSO_ITEM_TYPE && itemNiv->posx==enemigo->posX && itemNiv->posy==enemigo->posY);
	}

	pthread_mutex_lock(&semItems);

	do {
		enemigo->posX = 1 + (rand() % enemigo->pNivel->maxCols);
		enemigo->posY = 1 + (rand() % enemigo->pNivel->maxRows);
		estaEnRecurso = list_any_satisfy(list_items,(void*)esUnRecurso);
	} while (estaEnRecurso);

	CreateEnemy(list_items, enemigo->ID, enemigo->posX, enemigo->posY);
	log_info(logger, "Se crea el enemigo %d", enemigo->ID);
	pthread_mutex_unlock(&semItems);


	bool movimientoAleatorio;

	while (1) {

		pthread_mutex_lock(&semItems);
		movimientoAleatorio = analizarMovimientoDeEnemigo();
		pthread_mutex_unlock(&semItems);

		if (movimientoAleatorio) { ////MOVIMIENTO EN L
			/*
			 * Para hacer el movimiento de caballo uso la var ultimoMov que puede ser:
			 * 		a:el ultimo movimiento fue horizontal por primera vez
			 * 		b:el ultimo movimiento fue horizontal por segunda vez
			 * 		c:el ultimo movimiento fue vertical por primera vez
			 *
			 * La variable dirMov que indicara en que direccion se esta moviendo
			 * 		a:abajo-derecha
			 * 		b:abajo-izquierda
			 * 		c:arriba-derecha
			 * 		d:arriba-izquierda
			*/
			if(enemigo->posY<1){ //se esta en el limite vertical superior
				if((enemigo->posX<1)||(dirMov=='c'))
					dirMov='a';
				if((enemigo->posX>enemigo->pNivel->maxCols)||(dirMov=='d'))
					dirMov='b';
			}
			if(enemigo->posY>enemigo->pNivel->maxRows){ //se esta en el limite vertical inferior
				if((enemigo->posX<1)||(dirMov=='a'))
					dirMov='c';
				if((enemigo->posX>enemigo->pNivel->maxCols)||(dirMov=='b'))
					dirMov='d';
			}
			if(enemigo->posX<=0){ //se esta en el limite horizontal izquierdo
				if(dirMov=='b')
					dirMov='a';
				else
					dirMov='c';
			}
			if(enemigo->posX>enemigo->pNivel->maxCols){
				if(dirMov=='a')
					dirMov='b';
				else
					dirMov='d';
			}
			//calculando el movimiento segun lo anterior y la direccion con la que viene
			switch(dirMov){
			case'a':
				if(ultimoMov=='a'){
					dirMovimiento = derecha;
					ultimoMov='b';
				}else{
					if(ultimoMov=='c'){
						dirMovimiento = derecha;
						ultimoMov='a';
					}else{
						dirMovimiento = abajo;
						ultimoMov='c';
					}
				}
				break;
			case'b':
				if(ultimoMov=='a'){
					dirMovimiento = izquierda;
					ultimoMov='b';
				}else{
					if(ultimoMov=='c'){
						dirMovimiento = izquierda;
						ultimoMov='a';
					}else{
						dirMovimiento=abajo;
						ultimoMov='c';
					}
				}
				break;
			case 'c':
				if(ultimoMov=='a'){
					dirMovimiento=derecha;
					ultimoMov='b';
				}else{
					if(ultimoMov=='c'){
						dirMovimiento=derecha;
						ultimoMov='a';
					}else{
						dirMovimiento=arriba;
						ultimoMov='c';
					}
				}
				break;
			case 'd':
				if(ultimoMov=='a'){
					dirMovimiento=izquierda;
					ultimoMov='b';
				}else{
					if(ultimoMov=='c'){
						dirMovimiento=izquierda;
						ultimoMov='a';
					}else{
						dirMovimiento=arriba;
						ultimoMov='c';
						}
					}
				break;
			}
			actualizaPosicion(dirMovimiento, &(enemigo->posX),&(enemigo->posY));
			void esUnRecurso(ITEM_NIVEL *item){
				if ((item->item_type==RECURSO_ITEM_TYPE) && ((item->posx==enemigo->posX) && (item->posy==enemigo->posY))) {
					if (ultimoMov=='a'||ultimoMov=='b') {
						enemigo->posY++;
					} else {
						enemigo->posX--;
					}
				}
			}

			////PERSECUCION DE PERSONAJE
			pthread_mutex_lock(&semItems);
			list_iterate(list_items,(void*)esUnRecurso);
			MoveEnemy(list_items, enemigo->ID, enemigo->posX,enemigo->posY);
			nivel_gui_dibujar(list_items, enemigo->pNivel->nombre);
			pthread_mutex_unlock(&semItems);
			usleep(enemigo->pNivel->sleepEnemigos);


		} else { ////PERSECUCION DE PERSONAJE

			if(hayQueAsesinar){
				pthread_mutex_lock(&semItems);

				persVictima = asignarPersonajeVictima(enemigo);

				if(persVictima!=NULL && !persVictima->bloqueado){

					acercarmeALaVictimaPersonaje(enemigo, persVictima, &dirMovimiento);

					actualizaPosicion(dirMovimiento, &(enemigo->posX),&(enemigo->posY));

					evitarRecurso(enemigo);

					evitarOrigen(enemigo);

					MoveEnemy(list_items, enemigo->ID, enemigo->posX,enemigo->posY);
					nivel_gui_dibujar(list_items, enemigo->pNivel->nombre);

					if(estoyArriba(enemigo, persVictima) || hayAlgunEnemigoArriba(enemigo->pNivel, persVictima->posicion.x, persVictima->posicion.y)){
						ITEM_NIVEL *item = getItemById(persVictima->simbolo);
						item->num_enemy = -99; //Trampita
						persVictima->muerto=true;
						hayQueAsesinar = false;
					}
				}

				pthread_mutex_unlock(&semItems);
				usleep(enemigo->pNivel->sleepEnemigos);

			}

		}//Else->perseguir enemigos

	} //Fin de while(1)
	pthread_exit(NULL );
}
////FUNCIONES ENEMIGOS

_Bool alcanceVictima(tEnemigo *enemigo, ITEM_NIVEL *victima){
	return (victima->posx==enemigo->posX) && (victima->posy==enemigo->posY);
}


_Bool analizarMovimientoDeEnemigo(){

	int cantPersonajesNoBloqueados;
	bool personajeBloqueado(tPersonaje* personaje){
		return (personaje->bloqueado==false);
	}

	cantPersonajesNoBloqueados = list_count_satisfying(list_personajes,(void*)personajeBloqueado);

	return (cantPersonajesNoBloqueados==0);
}
//Devuelve true si alcance la victima y false en caso contrario

_Bool acercarmeALaVictima(tEnemigo *enemigo, ITEM_NIVEL *item, tDirMovimiento *dirMovimiento){

	//Elijo el eje por el que me voy a acercar
	if(enemigo->posY == item->posy){
		if(enemigo->posX < item->posx){
			*dirMovimiento = derecha;
		}
		if(enemigo->posX > item->posx){
			*dirMovimiento = izquierda;
		}
		if(enemigo->posX == item->posx){
			return true;
		}
	}
	else{ //acercarse por fila
		if(enemigo->posY < item->posy)
			*dirMovimiento = abajo;
		if(enemigo->posY > item->posy)
			*dirMovimiento = arriba;
	}
	return false;

}


_Bool acercarmeALaVictimaPersonaje(tEnemigo *enemigo, tPersonaje *personaje, tDirMovimiento *dirMovimiento){

	//Elijo el eje por el que me voy a acercar
	if(enemigo->posY == personaje->posicion.y){
		if(enemigo->posX < personaje->posicion.x){
			*dirMovimiento = derecha;
		}
		if(enemigo->posX > personaje->posicion.x){
			*dirMovimiento = izquierda;
		}
		if(enemigo->posX == personaje->posicion.x){
			return true;
		}
	}
	else{ //acercarse por fila
		if(enemigo->posY < personaje->posicion.y)
			*dirMovimiento = abajo;
		if(enemigo->posY > personaje->posicion.y)
			*dirMovimiento = arriba;
	}
	return false;

}


ITEM_NIVEL *asignarVictima(tEnemigo *enemigo){

	int dist2=999999;
	int distFinal;
	int i;
	ITEM_NIVEL *itemReturn = NULL;
	for(i=0;i<list_size(list_items);i++){
		ITEM_NIVEL *item=list_get(list_items,i);
		if(esUnPersonaje(item)){
			distFinal = calcularDistancia(enemigo, item->posx, item->posy);
			if((distFinal<dist2)){
				itemReturn = item;
				dist2=distFinal;
			}
		}
	}
	return itemReturn;
}


tPersonaje *asignarPersonajeVictima(tEnemigo *enemigo){

	int dist2=999999;
	int distFinal;
	int i;
	tPersonaje *personajeReturn = NULL;
	for(i=0;i<list_size(list_personajes);i++){

		tPersonaje *personaje=list_get(list_personajes, i);
		distFinal = calcularDistancia(enemigo, personaje->posicion.x, personaje->posicion.y);
		if((distFinal<dist2)){
			personajeReturn = personaje;
			dist2=distFinal;
		}

	}
	return personajeReturn;
}


tPersonaje *asignarVictimaVersionVieja(tEnemigo *enemigo){

	int i;
	tPersonaje *personaje, *personajeVictima;
	int posPerX;
	int posPerY;
	int distVictima=0, distancia;
	for (i = 0; i < list_size(list_personajes); i++) {
		personaje = (tPersonaje *) list_get_data(list_personajes, i);

		//Obtengo la posicion del personaje
		getPosPersonaje(list_items, personaje->simbolo, &posPerX, &posPerY);

		distancia = calcularDistancia(enemigo, posPerX, posPerY);
		//Calculo la distancia entre ambos
		if (i == 0 || distVictima>=distancia) {
			personajeVictima = personaje;
			distVictima = distancia;
			personajeVictima->posicion.x = posPerX;
			personajeVictima->posicion.y = posPerY;
		}

	}
	return personajeVictima;
}


_Bool esUnPersonaje(ITEM_NIVEL *item){
	return (item->item_type==PERSONAJE_ITEM_TYPE);

}


int calcularDistancia(tEnemigo *enemigo, int posX, int posY){


	int terminoEnX = abs(enemigo->posX - posX);
	int terminoEnY = abs(enemigo->posY - posY);

	return (terminoEnX + terminoEnY);

}
//Buscar en list_items y me devuelve el personaje que cumple la condicion

ITEM_NIVEL *getItemVictimaSiExiste(tEnemigo *enemigo){
	ITEM_NIVEL *personajeItem;
	bool esUnPersonaje(ITEM_NIVEL *item){
		return (item->item_type==PERSONAJE_ITEM_TYPE)&&((item->posx==enemigo->posX)&&(item->posy==enemigo->posY));
	}
	personajeItem = list_find(list_items, (void *)esUnPersonaje);
	return personajeItem;
}
//Busca en la lista de items una victima

ITEM_NIVEL *getItemById(char id_victima){
	int i;
	for(i=0;i<list_size(list_items);i++){
		ITEM_NIVEL *item=list_get(list_items,i);
		if(item->id==id_victima)
			return item;
	}
	return NULL;
}
//Mueve el enemigo atras en x si se paro en un recurso

void evitarRecurso(tEnemigo *enemigo){

	void esUnRecurso(ITEM_NIVEL *item){
		if ((item->item_type==RECURSO_ITEM_TYPE)&&((item->posx==enemigo->posX)&&(item->posy==enemigo->posY)))
			enemigo->posX--;
	}
	list_iterate(list_items,(void*)esUnRecurso);
}


void evitarOrigen(tEnemigo *enemigo){

	if(enemigo->posX<=1 && enemigo->posY<=1){
		enemigo->posX++;
		enemigo->posY++;
	}
}


_Bool estoyArriba(tEnemigo *enemigo, tPersonaje *persVictima){
	return (enemigo->posX==persVictima->posicion.x)&&(enemigo->posY==persVictima->posicion.y);
}


_Bool hayAlgunEnemigoArriba(tNivel *pNivel, int posPerX, int posPerY) {

	int i;
	int posEnemyX, posEnemyY;
	for (i = 1; i <= pNivel->cantEnemigos; i++) {
		getPosEnemy(list_items, i, &posEnemyX, &posEnemyY);
		if (posPerX == posEnemyX && posPerY == posEnemyY)
			return true;
	}
	return false;
}

//Busca en la list_personajes
tPersonaje *getPersonajeBySymbol(tSimbolo simbolo){
	tPersonaje *unPersonaje;
	bool buscaPersonaje(tPersonaje* personaje){
		return (personaje->simbolo==simbolo);
	}
	unPersonaje=(tPersonaje *)list_find(list_personajes,(void*)buscaPersonaje);
	return unPersonaje;
}


void crearNuevoPersonaje (tSimbolo simbolo) {
	tPersonaje *pPersonajeNuevo = malloc(sizeof(tPersonaje));
	pPersonajeNuevo->simbolo = simbolo;
	pPersonajeNuevo->bloqueado = false;
	pPersonajeNuevo->muerto = false;
	pPersonajeNuevo->posicion.x = 0;
	pPersonajeNuevo->posicion.y = 0;
	pPersonajeNuevo->recursos = list_create();
	list_add(list_personajes, pPersonajeNuevo);
	CrearPersonaje(list_items, (char)simbolo, INI_X, INI_Y);
	log_info(logger, "<<< Se agrego al personaje %c a la lista", simbolo);
}


void notificacionAPlataforma(int iSocket, tMensaje tipoMensaje, char *msjInfo) {
	tPaquete paquete;
	paquete.type   = tipoMensaje;
	paquete.length = 0;
	enviarPaquete(iSocket, &paquete, logger, msjInfo);
}


void calcularMovimiento(tNivel *pNivel, tDirMovimiento direccion, int *posX, int *posY) {
	switch (direccion) {
		case arriba:
			if (*posY > 1) {
				(*posY)--;
			}
			break;
		case abajo:
			if (*posY < pNivel->maxRows) {
				(*posY)++;
			}
			break;
		case izquierda:
			if (*posX > 1) {
				(*posX)--;
			}
			break;
		case derecha:
			if (*posX < pNivel->maxCols) {
				(*posX)++;
			}
			break;
		default:
			log_error(logger, "ERROR: no detecto una direccion de movimiento valida: %d", direccion);
			break;
	}
}


void matarPersonaje(tNivel *pNivel, tSimbolo *simboloItem, tMensaje tipoMensaje){

	tPaquete paquete;

	log_debug(logger, "Eliminando al personaje...");
	bool buscarPersonaje(tPersonaje* pPersonaje) {
		return (pPersonaje->simbolo == *simboloItem);
	}

	tPersonaje *personajeOut = list_remove_by_condition(list_personajes,(void*)buscarPersonaje);

	if (personajeOut == NULL) {
		log_debug(logger, "No se encontro el personaje");
	}

	BorrarPersonaje(list_items, *simboloItem);

	paquete.type=tipoMensaje;
	memcpy(paquete.payload, simboloItem,sizeof(tSimbolo));
	paquete.length=sizeof(tSimbolo);
	char *messageInfo = malloc(80);
	sprintf(messageInfo, "%s: Notifico a plataforma la muerte del personaje %c", pNivel->nombre, *simboloItem);
	enviarPaquete(pNivel->plataforma.socket,&paquete,logger, messageInfo);
	free(messageInfo);
	personaje_destroyer(personajeOut);
}


_Bool tieneLoQueNecesito(tPersonaje* pPersonaje1, tPersonaje* pPersonaje2) {		//--Electrolitos
	char* blkB = list_get(pPersonaje2->recursos, list_size(pPersonaje2->recursos) - 1); //--Guardar recurso por el que se bloquée B
	log_trace(logger, "Se esta buscando %c", *blkB);

	int contRec;
	char* levantadorA;
	for (contRec = 0; contRec < list_size(pPersonaje1->recursos) - (pPersonaje1->bloqueado ? 1 : 0); contRec++) {
		levantadorA = list_get(pPersonaje1->recursos, contRec);
		log_trace(logger, "\t\t%c tiene: %c", pPersonaje1->simbolo, *levantadorA);
		if (*blkB == *levantadorA)
			return true;
	}
	return false;
}

void *deteccionInterbloqueo(void* parametro) {

	tNivel *pNivel;
	pNivel = (tNivel *)parametro;

	extern t_list* list_items;
	t_list * bloqueados;

//	tSimbolo* caja;
//	tPersonaje *personaje;
//	tSimbolo personajeSimbolo;

	tInfoInterbloqueo deadlock = pNivel->deadlock;

	int cantBloq = 0;

// Iteramos infinitamente
	while (1) {
		pthread_mutex_lock(&semItems);
		if(analizarDeadlock){
			cantBloq   = 0;
			bloqueados = list_create();

			int contPer1, contPer2;
			tPersonaje *levantador1, *levantador2;

			//--Recorrer los personajes
			for (contPer1 = 0; contPer1 < list_size(list_personajes); contPer1++) {
				levantador1 = list_get(list_personajes, contPer1);

				//--marca a los que no estan bloqueados
				if (levantador1->bloqueado) {
					levantador1->marcado = false;
					cantBloq++;
				} else {
					levantador1->marcado = true;
				}
			}

			for (; cantBloq >= 0; cantBloq--) { //(En el peor de los casos, tiene que asignar 2n-1 veces)
				//Por cada pj no marcado
				for (contPer1 = 0; contPer1 < list_size(list_personajes); contPer1++) {
					levantador1 = list_get(list_personajes, contPer1);
					if (!levantador1->marcado) {
						//Si necesita un recurso de uno marcado
						for (contPer2 = 0; contPer2 < list_size(list_personajes); contPer2++) {
							levantador2 = list_get(list_personajes, contPer2);

							if (levantador2->marcado && tieneLoQueNecesito(levantador2, levantador1)) {
								log_trace(logger, "Marque a %c", levantador1->simbolo);
								levantador1->marcado = true;
								break;
							}
						}
					}
				}

			};

	//		bool personajeBloqueado(tPersonaje* personaje){
	//			return (personaje->marcado==false);
	//		}
	//		int cant = list_count_satisfying(list_personajes,(void*)personajeBloqueado);


			//Estan en DeadLock los que no esten marcados
			for (contPer1 = 0; contPer1 < list_size(list_personajes); contPer1++) {
				levantador1 = list_get(list_personajes, contPer1);
				if (!levantador1->marcado) {
					//Agrego solo el simbolo, porque si agregaba el personaje levantador1 completo andaba mal y no agregaba bien.
					list_add_new(bloqueados, &levantador1->simbolo, sizeof(list_personajes));
					log_trace(logger, "%c esta en Deadlock", levantador1->simbolo);
				}
			}

			//--Si la lista tiene más de 1 deadlockeados elijo uno y le notifico al planificador
			if ((list_size(bloqueados) > 1) && (deadlock.recovery)) {
				//--Envía un header con la cantidad de personajes
				tSimbolo *simboloPersBlock = list_get(bloqueados, 0);

				log_info(logger, "Hay deadlock y voy a matar a %c", *simboloPersBlock);
				//No analices deadlock hasta que hayas liberado recursos
				analizarDeadlock = false;
				tPaquete paquete;
				serializarSimbolo(N_MUERTO_POR_DEADLOCK, *simboloPersBlock, &paquete);
				enviarPaquete(pNivel->plataforma.socket, &paquete, logger, "Envio a plataforma el personaje que murio");
				log_info(logger, "El personaje %c se elimino por participar en un interbloqueo", *simboloPersBlock);

				list_destroy_and_destroy_elements(bloqueados, free);
			}

			// Mandamos el proceso a dormir para que espere el tiempo definido por archivo de config.
		}
		pthread_mutex_unlock(&semItems);
		usleep(deadlock.checkTime);
		log_trace(logger, ">>>Revisando DL<<<");
	}
	pthread_exit(NULL);
}

void actualizaPosicion(tDirMovimiento dirMovimiento, int *posX, int *posY) {

	switch (dirMovimiento) {
	case derecha:
		(*posX)++; //DERECHA
		break;
	case arriba:
		(*posY)--; //ARRIBA
		break;
	case izquierda:
		(*posX)--; //IZQUIERDA
		break;
	case abajo:
		(*posY)++; //ABAJO
		break;
	default:
		break;
	}
}

//--------------------------------------Señal SIGINT
void cerrarForzado(int sig) {
	cerrarNivel("Cerrado Forzoso Nivel.");
}

void cerrarNivel(char* msjLog) {
	log_trace(logger, msjLog);
	nivel_gui_terminar();
	printf("%s\n", msjLog);
	exit(EXIT_FAILURE);
}
//--------------------------------------Señal SIGINT

static void personaje_destroyer(tPersonaje *personaje) {
	list_destroy_and_destroy_elements(personaje->recursos, free);
	free(personaje);
}
