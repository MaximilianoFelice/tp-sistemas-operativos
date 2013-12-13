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

t_log  *logger = NULL;
t_list *list_personajes = NULL;
t_list *list_items = NULL;

pthread_mutex_t semSockPaq;
pthread_mutex_t semItems;

void handshakeConPlataforna(int iSocket, tNivel *pNivel) {
	tPaquete paquete;
	int largoNombre = strlen(pNivel->nombre) + 1;
	paquete.type   = N_HANDSHAKE;
	paquete.length = largoNombre;
	memcpy(paquete.payload, pNivel->nombre, largoNombre);
	enviarPaquete(iSocket, &paquete, logger, "Se envia handshake a plataforma");

	tMensaje tipoDeMensaje;
	char* payload;

	recibirPaquete(iSocket,&tipoDeMensaje,&payload,logger,"Se recibe handshake de plataforma");

	if (tipoDeMensaje == PL_NIVEL_YA_EXISTENTE) {

	} else {
		cerrarNivel("Tipo de mensaje incorrecto (Se esperaba PL_HANDSHAKE)");
	}

	tInfoNivel infoDeNivel;
	tipoDeMensaje=N_DATOS;
	infoDeNivel.algoritmo = pNivel->plataforma.algPlanif;
	infoDeNivel.quantum	  = pNivel->plataforma.valorAlgorimo;
	infoDeNivel.delay	  = pNivel->plataforma.delay;
	serializarInfoNivel(N_DATOS, infoDeNivel, &paquete);

	enviarPaquete(iSocket, &paquete, logger, "Se envia a la plataforma los criterios de planificacion");
}


void crearEnemigos(tNivel nivel) {
	// CREANDO Y LANZANDO HILOS ENEMIGOS
	if (nivel.cantEnemigos > 0) {
		int indexEnemigos;
		tEnemigo *aHilosEnemigos;
		aHilosEnemigos = calloc(nivel.cantEnemigos, sizeof(tEnemigo));

		for (indexEnemigos = 0; indexEnemigos < nivel.cantEnemigos; indexEnemigos++) {
			aHilosEnemigos[indexEnemigos].number = indexEnemigos + 1; //El numero o id de enemigo

			if (pthread_create(&aHilosEnemigos[indexEnemigos].thread_enemy, NULL, enemigo,(void*) &aHilosEnemigos[indexEnemigos])) {
				log_error(logger, "pthread_create: %s", strerror(errno));
				exit(EXIT_FAILURE);
			}
		}

	} else {
		nivel_gui_dibujar(list_items, nivel.nombre);
	}
}

void conexionPersonaje(int iSocket, char *sPayload) {
	tPaquete paquete;
	tPersonaje *pPersonaje;
	tMovimientoPers movPersonaje;

	pPersonaje = getPersonajeBySymbol((int8_t)*sPayload);
	free(sPayload);

	if (tPersonaje == NULL) {
		crearNuevoPersonaje(movPersonaje.simbolo);
		notificacionAPlataforma(iSocket, &paquete, N_CONEXION_EXITOSA, "Se notifica a plataforma que el personaje se onecto exitosamente");
	} else {//se encontro=>el personaje ya existe
		notificacionAPlataforma(iSocket, &paquete, N_PERSONAJE_YA_EXISTENTE, "Se notifica a plataforma que el personaje ya existe");
	}
}

void movimientoPersonaje(int iSocket, char *sPayload) {
	tPaquete paquete;
	tPersonaje *pPersonaje;
	tPosicion posPersonaje;
	tMovimientoPers* movPersonaje;

	movPersonaje = deserializarMovimientoPers(sPayload);
	free(sPayload);

	log_debug(logger, "<<< El personaje %c solicito moverse", movPersonaje->simbolo);

	pPersonaje = getPersonajeBySymbol(movPersonaje->simbolo);

	if (pPersonaje != NULL && !pPersonaje->muerto) {
		pPersonaje->bloqueado=false;

		getPosPersonaje(list_items, movPersonaje->simbolo, &posPersonaje.x, &posPersonaje.y);
		calcularMovimiento(movPersonaje.direccion, &posPersonaje.x, &posPersonaje.y);

		pthread_mutex_lock(&semItems);
		MoverPersonaje(list_items, movPersonaje->simbolo, &posPersonaje.x, &posPersonaje.y);
		pthread_mutex_unlock(&semItems);
		notificacionAPlataforma(iSocket, &paquete, N_CONFIRMACION_MOV, "Notificando a plataforma personaje movido correctamente");

	} else {
		notificacionAPlataforma(iSocket, &paquete, N_PERSONAJE_INEXISTENTE, "Notificando a plataforma personaje no existe");
	}
}

void posicionRecurso(int iSocket, char *sPayload) {
	tPregPosicion *posConsultada;
	tPaquete paquete;
	posConsultada = deserializarPregPosicion(sPayload);
	free(sPayload);

	log_debug(logger, "<<< Personaje %c solicita la posicion del recurso %c", posConsultada->simbolo, posConsultada->recurso);
	bool buscarRecurso(ITEM_NIVEL *item){
		return ((item->id == posConsultada->recurso) && (item->item_type == RECURSO_ITEM_TYPE));
	}

	ITEM_NIVEL* pRecurso;
	pRecurso = list_find(list_items, (void*)buscarRecurso);

	if (pRecurso != NULL) {
		pthread_mutex_lock(&semSockPaq);
		tRtaPosicion rtaPosicion;
		rtaPosicion.posX = pRecurso->posx;
		rtaPosicion.posY = pRecurso->posy;
		pthread_mutex_lock(&semSockPaq);
		serializarRtaPosicion(N_POS_RECURSO, rtaPosicion, &paquete);

		enviarPaquete(iSocket, &paquete, logger, "Se envia posicion de recurso a la plataforma");
		pthread_mutex_unlock(&semSockPaq);

	} else {
		notificacionAPlataforma(iSocket, &paquete, N_RECURSO_INEXISTENTE, "El recurso solicitado no existe");
	}
}

void solicitudRecurso(int iSocket, char *sPayload) {
	tPregPosicion *posConsultada;
	tPaquete paquete;
	posConsultada = deserializarPregPosicion(sPayload);
	free(sPayload);

	log_debug(logger, "<<< Personaje %c solicita una instancia del recurso %c", (char)posConsultada->simbolo, (char)posConsultada->recurso);
	// Calculo la cantidad de instancias
	pthread_mutex_lock(&semItems);
	int cantInstancias = restarInstanciasRecurso(list_items, posConsultada->recurso);
	pthread_mutex_unlock(&semItems);

	tPersonaje *pPersonaje;
	pPersonaje = getPersonajeBySymbol(posConsultada->simbolo);

	if (cantInstancias >= 0) { //SE LO OTORGO EL RECURSO PEDIDO

		log_info(logger, "Al personaje %c se le dio el recurso %c",posConsultada->simbolo,posConsultada->recurso);

		//Agrego el recurso a la lista de recursos del personaje y lo desbloquea si estaba bloqueado
		void agregaRecursoYdesboquea(tPersonaje *personaje){
			if (personaje->simbolo == pPersonaje->simbolo) {
				personaje->bloqueado = false;
				list_add_new(personaje->recursos, &(posConsultada->recurso), sizeof(tSimbolo));
			}
		}

		list_iterate(list_personajes,(void*) agregaRecursoYdesboquea);
		pthread_mutex_lock(&semSockPaq);
		serializarSimbolo(N_ENTREGA_RECURSO, posConsultada->recurso, &paquete);
		// Envio mensaje donde confirmo la otorgacion del recurso pedido
		enviarPaquete(iSocket, &paquete, logger, "Se envia confirmacion de otorgamiento de recurso a plataforma");
		pthread_mutex_unlock(&semSockPaq);

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
	}
}


void desconexionPersonaje(char *sPayload) {
	tDesconexionPers *persDesconectado;
	persDesconectado = deserializarDesconexionPers(sPayload);
	free(sPayload);

	log_debug(logger, "<<< El personaje %c se desconecto", persDesconectado->simbolo);

	// Eliminar al personaje de list_personajes
	bool buscarPersonaje(tPersonaje* pPersonaje) {
		return (pPersonaje->simbolo == persDesconectado->simbolo);
	}

	tPersonaje *personajeOut = list_remove_by_condition(list_personajes,(void*)buscarPersonaje);
	pthread_mutex_lock(&semItems);
	//agrego una instancia a list_items de todos los recursos que me manda planificador (que son todos los que no reasigno)
	int iIndexRecurso;
	for (iIndexRecurso=0; iIndexRecurso<persDesconectado->lenghtRecursos; iIndexRecurso++) {
		sumarRecurso(list_items, persDesconectado->recursos[iIndexRecurso]);
		log_debug(logger, "Libere una instancia del recurso %c", persDesconectado->recursos[iIndexRecurso]);
	}

	if (!personajeOut->muerto) {
		BorrarItem(list_items, persDesconectado->simbolo); //Si no esta muerto, sacalo
	} else {
		log_error(logger, "No se elimina al personaje %c", personajeOut->simbolo);
	}
	pthread_mutex_unlock(&semItems);
	log_debug(logger, "Libere recursos");
	free(persDesconectado);
	personaje_destroyer(personajeOut);
}


int main(int argc, char** argv) {
	tNivel nivel;
	tInfoInterbloqueo interbloqueo;
	int iSocket;
	char bufferInotify[TAM_BUFFER];
	int rv, descriptorVigilador, bytesLeidos, descriptorInotify;

	signal(SIGINT, cerrarForzado);

	pthread_mutex_init(&semSockPaq, NULL);
	pthread_mutex_init(&semItems,NULL);

	tPosicion posPersonaje;
	posPersonaje.x = 0;
	posPersonaje.y = 0;

	tRtaPosicion posRespondida;

	//LOG
	logger = logInit(argv, "NIVEL");

	/*
	 * FUNCION INIT
	 */
	// INICIALIZANDO GRAFICA DE MAPAS
	list_items 	    = list_create();
	list_personajes = list_create();
	nivel_gui_inicializar();
	nivel_gui_get_area_nivel(&nivel.maxRows, &nivel.maxRows);
	levantarArchivoConf(argv[1], &nivel, &interbloqueo);

	//SOCKETS
	iSocket = connectToServer(nivel.plataforma.IP, nivel.plataforma.port, logger);
    if (iSocket == EXIT_FAILURE) {
    	cerrarNivel("No se puede conectar con la plataforma");
    }

    handshakeConPlataforna(iSocket, &nivel);

	//INOTIFY
	descriptorInotify=inotify_init();
	descriptorVigilador=inotify_add_watch(descriptorInotify,argv[1],IN_MODIFY);
	if (descriptorVigilador==-1) {
		log_error(logger, "error en inotify add_watch");
	}

	//POLL
	uDescriptores[0].fd=sockete;
	uDescriptores[0].events=POLLIN;
	uDescriptores[1].fd=descriptorInotify;
	uDescriptores[1].events=POLLIN;

	crearEnemigos(nivel);

	//LANZANDO EL HILO DETECTOR DE INTERBLOQUEO
	pthread_t hiloInterbloqueo;
	pthread_create(&hiloInterbloqueo,NULL,&deteccionInterbloqueo,NULL);

	//WHILE PRINCIPAL
	while (1) {
		if ((rv=poll(uDescriptores,2,-1))==-1) {
			perror("poll");
		} else {
			if (uDescriptores[1].revents&POLLIN) {
				i = 0;
				bytesLeidos = read(descriptorInotify,bufferInotify,TAM_BUFFER);
				while (i<bytesLeidos) {
					struct inotify_event* evento;
					evento = (struct inotify_event*) &bufferInotify[i];
					if (evento->mask & IN_MODIFY) {//avisar a planificador que cambio el archivo config
						actualizarInfoNivel(argv[1]);
						//ENVIANDO A PLATAFORMA NOTIFICACION DE ALGORITMO ASIGNADO
						tInfoNivel infoDeNivel;
						infoDeNivel.algoritmo=algoritmo;
						infoDeNivel.quantum=quantum;
						infoDeNivel.delay=retardo;
						//serializacion propia porque la de protocolo no me andaba bien
						pthread_mutex_lock(&semSockPaq);
						serializarInfoNivel(N_ACTUALIZACION_CRITERIOS, infoDeNivel, &paquete);
						enviarPaquete(sockete,&paquete,logger,"notificando a plataforma algoritmo");
						pthread_mutex_unlock(&semSockPaq);
					}
					i+=TAM_EVENTO+evento->len;
				}
			}//Preguntar por que si poll detecta actividad en el descriptor del inotify y este solo se acciona cuando ocurre in_modify => haria falta todo lo que sigue? o
			 //simplemente podria actualizar los datos y listo?
			if (uDescriptores[0].revents & POLLIN) {
				tPregPosicion* posConsultada = NULL;
				pers_t* personaG = NULL;
				ITEM_NIVEL* itemRec = NULL;
				tDesconexionPers* persDesconectado;
				recibirPaquete(sockete, &tipoDeMensaje, &sPayload, logger,"Recibiendo mensaje de plataforma");

				switch(tipoDeMensaje){
				case PL_CONEXION_PERS:
					conexionPersonaje(iSocket, sPayload);
					break;

				case PL_MOV_PERSONAJE:
					movimientoPersonaje(iSocket, sPayload);
					break;

				case PL_POS_RECURSO:
					posicionRecurso(iSocket, sPayload);
					break;

				case PL_SOLICITUD_RECURSO:
					solicitudRecurso(iSocket, sPayload);
					break;

				case PL_DESCONEXION_PERSONAJE:// Un personaje termino o murio y debo liberar instancias de recursos que tenia asignado
					desconexionPersonaje(sPayload)
					break;
				} //Fin del switch

				pthread_mutex_lock(&semItems);
				nivel_gui_dibujar(list_items, nivel.nombre);

				if (tipoMsj==PL_MOV_PERSONAJE) {
					personaG->listoParaPerseguir = true;
				}
				pthread_mutex_unlock(&semItems);
			}
		}
	}

	inotify_rm_watch(descriptorInotify,descriptorVigilador);
	close(descriptorInotify);
	log_destroy(logger);
	return EXIT_SUCCESS;
}

void levantarArchivoConf(char* pathConfigFile, tNivel *pNivel, tInfoInterbloqueo *pInterbloqueo) {

	t_config *configNivel;
	tPosicion posCaja;
	int iCaja = 1;
	bool hayCajas = false;
	char* sLineaCaja;
	char** aCaja;
	char** dirYpuerto;
	char* datosPlataforma;

	configNivel = config_create(pathConfigFile);

	log_info(logger, "Verificando archivo de configuracion...");
	char* sParametros = "Nombre,Recovery,Enemigos,Sleep_Enemigos,Algoritmo,Quantum,Retardo,TiempoChequeoDeadlock,Plataforma";
    char* token;
    token = strtok(sParametros, ",");

    while (token) {

    	if (!(config_has_property(configNivel, token))) {
    		char *msjError;
    		msjError = string_from_format("[ERROR] No se encontro '%s' en '%s'\n", token, pathConfigFile);
    		cerrarNivel(msjError);
		}

        token = strtok(NULL,","); //Pasa al siguiente token
    }

    free(sParametros);

	sLineaCaja = string_from_format("Caja%i", iCaja);

	while (config_has_property(configNivel, sLineaCaja)) {
		hayCajas = true;
		aCaja = config_get_array_value(configNivel, sLineaCaja);
		free(sLineaCaja);

		posCaja.x  = atoi(aCaja[3]);
		posCaja.y  = atoi(aCaja[4]);

		if (posCaja.y > pNivel->maxRows || posCaja.y > pNivel->maxCols || posCaja.y < 1 || posCaja.y < 1) {
			char* messageLimitErr;
			messageLimitErr = string_from_format(
				"La caja %s excede los limites de la pantalla. CajaPos=(%d,%d) - Limites=(%d,%d)",
				iCaja, posCaja.x, posCaja.y, pNivel->maxCols, pNivel->maxRows
			);
			cerrarNivel(messageLimitErr);
		}

		pthread_mutex_lock(&semItems);
		// Si la validacion fue exitosa creamos la caja de recursos
		CrearCaja(list_items, *aCaja[1],atoi(aCaja[3]),atoi(aCaja[4]),atoi(aCaja[2]));
		pthread_mutex_unlock(&semItems);

		iCaja++;
		sLineaCaja = string_from_format("Caja%i", iCaja);
	};

	if (!hayCajas) {
		cerrarNivel("No hay cajas disponibles");
	}

	log_info(logger, "Archivo correcto, se procede a levantar los valores");

	pNivel->cantRecursos   = list_size(list_items);

	pInterbloqueo->recovery  = config_get_int_value(configNivel, "Recovery");
	pInterbloqueo->checkTime = config_get_int_value(configNivel, "TiempoChequeoDeadlock");

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

	free(algoritmoAux);

	pNivel->plataforma.delay = config_get_int_value(configNivel, "Retardo");

	datosPlataforma = config_get_string_value(configNivel, "Plataforma");

	char ** aDatosPlataforma = string_split(datosPlataforma, ":");
	pNivel->plataforma.IP    = aDatosPlataforma[0];
	pNivel->plataforma.port  = atoi(aDatosPlataforma[1]);

	config_destroy(configNivel);
}

void actualizarInfoNivel(char* argumento){
	extern tAlgoritmo algoritmo;
	extern int quantum;
	extern uint32_t retardo;
	char* algoritmoAux;
	t_config *configNivel;

	while(1){ //------------------------------------->despues de muchos dias de putear esta era la solucion (hay un retraso que produce un seg.fault)
		configNivel  = config_create(argumento);
		if(!config_has_property(configNivel,"Algoritmo")){
			usleep(10);

			config_destroy(configNivel);
		}
		else break;
	}


	algoritmoAux = config_get_string_value(configNivel, "Algoritmo");
	char *rr="RR";
	if (strcmp(algoritmoAux,rr)==0) {
		algoritmo = RR;
	} else {
		algoritmo = SRDF;
	}
	quantum = config_get_int_value(configNivel, "Quantum");
	retardo = config_get_int_value(configNivel, "Retardo");
	log_debug(logger,"se produjo un cambio en el archivo config, Algorimo: %s quantum:%i retardo:%i",algoritmoAux,quantum,retardo);
	config_destroy(configNivel);
}

void *enemigo(void * args) {
	enemigo_t *enemigo;
	enemigo = (enemigo_t *) args;
	extern t_list *list_items;
	extern tPaquete paquete;
	int cantPersonajesActivos;
	int contMovimiento = 3;
	char victimaAsignada='0';
	char ultimoMov='a';
	char dirMov='b';
	int dist1,dist2=9999999,i;
	ITEM_NIVEL* item;
	pers_t* persVictima;

	//chequeando que la posicion del enemigo no caiga en un recurso
	enemigo->posX = 1+(rand() % maxCols);
	enemigo->posY = 1+(rand() % maxRows);
	bool esUnRecurso(ITEM_NIVEL *itemNiv){ return (itemNiv->item_type==RECURSO_ITEM_TYPE&&itemNiv->posx==enemigo->posX&&itemNiv->posy==enemigo->posY);}
	while(list_any_satisfy(list_items,(void*)esUnRecurso)){
		enemigo->posX=1+(rand() % maxCols);
		enemigo->posY=1+(rand() % maxRows);
	}
	pthread_mutex_lock(&semItems);
	CreateEnemy(list_items,enemigo->num_enemy,enemigo->posX,enemigo->posY);
	pthread_mutex_unlock(&semItems);

	while (1) {
		bool personajeBloqueado(pers_t* personaje){return(personaje->bloqueado==false && personaje->muerto==false && personaje->listoParaPerseguir==true);}
		cantPersonajesActivos=list_count_satisfying(list_personajes,(void*)personajeBloqueado);
		if (cantPersonajesActivos == 0) {
			/* para hacer el movimiento de caballo uso la var ultimoMov que puede ser:
			 * a:el ultimo movimiento fue horizontal por primera vez b:el utlimo movimiento fue horizontal por segunda vez
			 * c:el ultimo movimiento fue vertical por primera vez
			 * y la variable dirMov que indicara en que direccion se esta moviendo
			 * a:abajo-derecha b:abajo-izquierda c:arriba-derecha d:arriba-izquierda
			*/
			if(enemigo->posY<1){ //se esta en el limite vertical superior
				if((enemigo->posX<1)||(dirMov=='c')) dirMov='a';
				if((enemigo->posX>maxCols)||(dirMov=='d')) dirMov='b';
			}
			if(enemigo->posY>maxRows){ //se esta en el limite vertical inferior
				if((enemigo->posX<1)||(dirMov=='a')) dirMov='c';
				if((enemigo->posX>maxCols)||(dirMov=='b'))dirMov='d';
			}
			if(enemigo->posX<=0){ //se esta en el limite horizontal izquierdo
				if(dirMov=='b') dirMov='a';else dirMov='c';
			}
			if(enemigo->posX>maxCols){
				if(dirMov=='a') dirMov='b';else dirMov='d';
			}
			//calculando el movimiento segun lo anterior y la direccion con la que viene
			switch(dirMov){
			case'a':
				if(ultimoMov=='a'){
					contMovimiento=1;
					ultimoMov='b';
				}else{
					if(ultimoMov=='c'){
						contMovimiento=1;
						ultimoMov='a';
					}else{
						contMovimiento=4;
						ultimoMov='c';
					}
				}
				break;
			case'b':
				if(ultimoMov=='a'){
					contMovimiento=3;
					ultimoMov='b';
				}else{
					if(ultimoMov=='c'){
						contMovimiento=3;
						ultimoMov='a';
					}else{
						contMovimiento=4;
						ultimoMov='c';
					}
				}
				break;
			case 'c':
				if(ultimoMov=='a'){
					contMovimiento=1;
					ultimoMov='b';
				}else{
					if(ultimoMov=='c'){
						contMovimiento=1;
						ultimoMov='a';
					}else{
						contMovimiento=2;
						ultimoMov='c';
					}
				}
				break;
			case 'd':
				if(ultimoMov=='a'){
					contMovimiento=3;
					ultimoMov='b';
				}else{
					if(ultimoMov=='c'){
						contMovimiento=3;
						ultimoMov='a';
					}else{
						contMovimiento=2;
						ultimoMov='c';
						}
					}
				break;
			}
			actualizaPosicion(&contMovimiento, &(enemigo->posX),&(enemigo->posY));
			void esUnRecurso(ITEM_NIVEL *ite){
				if ((ite->item_type==RECURSO_ITEM_TYPE)&&((ite->posx==enemigo->posX)&&(ite->posy==enemigo->posY))){
					if(ultimoMov=='a'||ultimoMov=='b')enemigo->posY++;
					else enemigo->posX--;
				}
			}
			list_iterate(list_items,(void*)esUnRecurso);
		}
		else { //ELEGIR O PERSEGUIR A LA VICTIMA

			if(victimaAsignada=='0'){//No tiene victima => selecciono una victima

				for(i=0;i<list_size(list_items);i++){
					item=list_get(list_items,i);
					dist1=(enemigo->posX-item->posx)*(enemigo->posX-item->posx)+(enemigo->posY-item->posy)*(enemigo->posY-item->posy);
					if((dist1<dist2)&&(item->item_type==PERSONAJE_ITEM_TYPE)){
						victimaAsignada=item->id;
						dist2=dist1;
					}
				}
			}
			else{//Ya tiene una victima asignada => busco la victimaAsignada en la lista de items y la coloco en persVictima

				persVictima = getPersonajeBySymbol((tSimbolo)victimaAsignada);

				if(persVictima->bloqueado || persVictima->muerto || !persVictima->listoParaPerseguir){
					//Si estaba bloqueado o ya matado(pero aun no lo saque) => busco nueva victima
					victimaAsignada='0';
				}
				else{ //Me acerco a la victima
					item = getItemById(victimaAsignada);

					//Elijo el eje por el que me voy a acercar
					if(enemigo->posY==item->posy){
						if(enemigo->posX<item->posx){
							contMovimiento=1;
						}
						if(enemigo->posX>item->posx){
							contMovimiento=3;
						}
						if(enemigo->posX==item->posx){//se esta en la misma posicion que la victima =>matarla
							//un semaforo para que no mande mensaje al mismo tiempo que otros enemigos o el while principal
							//otro semaforo para que no desasigne y se esten evaluando otros
							pers_t *unPersonaje = getPersonajeBySymbol((tSimbolo)item->id);
							if(!unPersonaje->muerto){ //Si no esta muerto, matar
								unPersonaje->muerto = true;
								matarPersonaje((tSimbolo *)&unPersonaje->simbolo);
							}
							victimaAsignada='0';
						}
					}
					else{ //acercarse por fila
						if(enemigo->posY<item->posy) contMovimiento=4;
						if(enemigo->posY>item->posy) contMovimiento=2;
				    }
				}
				//TODO agregar si se llega a "chocar" con un personaje que no es su victima-->no habia contemplado este caso

				actualizaPosicion(&contMovimiento, &(enemigo->posX),&(enemigo->posY));

				evitarRecurso(enemigo);

				ITEM_NIVEL *personajeItem = getVictima(enemigo);

				if(personajeItem!= NULL){
					pers_t *unPersonaje = getPersonajeBySymbol((tSimbolo)personajeItem->id);
					if(!unPersonaje->muerto){ //Si no esta muerto, matar
						unPersonaje->muerto = true;
						matarPersonaje((tSimbolo *)&unPersonaje->simbolo);
					}
				}
			}
		}

		pthread_mutex_lock(&semItems);
		MoveEnemy(list_items, enemigo->num_enemy, enemigo->posX,enemigo->posY);
		nivel_gui_dibujar(list_items, nom_nivel);
		pthread_mutex_unlock(&semItems);
		usleep(sleep_enemigos);

	} //Fin de while(1)
	pthread_exit(NULL );
}

//Buscar en list_items y me devuelve el personaje que cumple la condicion
ITEM_NIVEL *getVictima(enemigo_t *enemigo){
	ITEM_NIVEL *personajeItem;
	bool esUnPersonaje(ITEM_NIVEL *item){
		return (item->item_type==PERSONAJE_ITEM_TYPE)&&((item->posx==enemigo->posX)&&(item->posy==enemigo->posY));
	}
	personajeItem = list_find(list_items, (void *)esUnPersonaje);
	return personajeItem;
}

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
void evitarRecurso(enemigo_t *enemigo){

	void esUnRecurso(ITEM_NIVEL *iten){
		if ((iten->item_type==RECURSO_ITEM_TYPE)&&((iten->posx==enemigo->posX)&&(iten->posy==enemigo->posY)))
			enemigo->posX--;
	}
	list_iterate(list_items,(void*)esUnRecurso);
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

void crearNuevoPersonaje(tSimbolo simbolo) {
	tPersonaje *pPersonajeNuevo = malloc(sizeof(tPersonaje));
	pPersonajeNuevo->simbolo  = simbolo;
	pPersonajeNuevo->bloqueado  = false;
	pPersonajeNuevo->muerto = false;
	pPersonajeNuevo->listoParaPerseguir = false;
	pPersonajeNuevo->recursos = list_create();
	list_add(list_personajes, pPersonajeNuevo);
	pthread_mutex_unlock(&semItems);
	CrearPersonaje(list_items, (char)simbolo, INI_X, INI_Y);
	pthread_mutex_unlock(&semItems);
	log_info(logger, "<<< Se agrego al personaje %c a la lista", simbolo);
}

void notificacionAPlataforma(int iSocket, tPaquete *paquete, tMensaje tipoMensaje, char *msjInfo){
	pthread_mutex_lock(&semSockPaq);
	paquete->type   = tipoMensaje;
	paquete->length = 0;
	enviarPaquete(iSocket, paquete, logger, msjInfo);
	pthread_mutex_unlock(&semSockPaq);
}

void calcularMovimiento(tDirMovimiento direccion, int *posX, int *posY){
	switch (direccion) {
		case arriba:
			if (*posY > 1) (*posY)--;
		break;
		case abajo:
			if (*posY < maxRows) (*posY)++;
		break;
		case izquierda:
			if (*posX > 1) (*posX)--;
		break;
		case derecha:
			if (*posX < maxCols) (*posX)++;
		break;
		default:
			log_error(logger, "ERROR: no detecto una direccion de movimiento valida: %d", direccion);
		break;
	}
}

void matarPersonaje(tSimbolo *simboloItem){
	pthread_mutex_lock(&semItems);
	BorrarItem(list_items, (char)*simboloItem);
	pthread_mutex_unlock(&semItems);
	pthread_mutex_lock(&semSockPaq);
	paquete.type=N_MUERTO_POR_ENEMIGO;
	memcpy(paquete.payload, simboloItem,sizeof(tSimbolo));
	paquete.length=sizeof(tSimbolo);
	enviarPaquete(sockete,&paquete,logger,"enviando notificacion de muerte de personaje a plataforma");
	pthread_mutex_unlock(&semSockPaq);
}

void *deteccionInterbloqueo (void *parametro){
	extern t_list* list_items;
	extern tPaquete paquete;

	tSimbolo* caja;
	tSimbolo perso;

	int i,j,k,fila;
	ITEM_NIVEL* itemRec = NULL;
	pers_t* personaje   = NULL;
	struct timespec dormir;
	dormir.tv_sec  = (time_t)(timeCheck/1000);
	dormir.tv_nsec = (long)((timeCheck%1000) * 1000000);

	int cantPersonajes;
	int **matAsignacion = NULL;
	int **matSolicitud  = NULL;
	int *vecSatisfechos = NULL;
	t_caja *vecCajas    = NULL;
	int indice,cantPersonajesSatisfechos;
	char encontrado;

	while(1){
	    cantPersonajes = list_size(list_personajes);
		indice=0;
		cantPersonajesSatisfechos=0;

		if (cantPersonajes!=0) {
			matAsignacion = (int**) malloc(sizeof(int*) * cantPersonajes);
			matSolicitud  = (int**) malloc(sizeof(int*) * cantPersonajes);
			vecSatisfechos= (int*)  malloc(sizeof(int)  * cantPersonajes);
			vecCajas = (t_caja*) malloc(sizeof(t_caja) * cantRecursos);

			for (i=0; i<cantPersonajes; i++) {
				matAsignacion[i] = (int*) malloc(sizeof(int) * cantRecursos);
				matSolicitud[i]  = (int*) malloc(sizeof(int) * cantRecursos);
			}

		    pthread_mutex_lock (&semItems);// Nadie mueve un pelo hasta que no se evalue el interbloqueo
			// Inicializando matrices
			for (i=0; i<cantPersonajes; i++) {

				for (j=0; j<cantRecursos; j++) {
					matAsignacion[i][j] = 0;
					*(*(matSolicitud+i)+j) = 0;
				}
				*(vecSatisfechos+i)=-1;
			}
			// Llenando las matrices con la info
			for (i=0; i<list_size(list_items); i++) {
				itemRec = list_get(list_items,i);

				if (itemRec->item_type == RECURSO_ITEM_TYPE) {
					vecCajas[indice].simbolo = itemRec->id;
					vecCajas[indice].cantidadInstancias = itemRec->quantity;
					indice++;
				}
			}

			for(i=0; i<cantPersonajes; i++) { // Recorro la lista de personajes y por cada personaje
				personaje=list_get(list_personajes,i);

				for (j=0; j<list_size(personaje->recursos); j++) { // Recorro su lista de recursos y por cada recurso asignados:
					caja = list_get(personaje->recursos,j);

					for (k=0; k<cantRecursos; k++) {               // Recorro a vecCajas (vector de recursos en list_items) viendo por c/u de sus elementos
						if (vecCajas[k].simbolo == (char)*caja) {  // Si coincide con el recurso recorrido con j (esto es para que las matrices mantengan los indices)
							// Me fijo si es el ultimo elemento de la lista(=>es el por el que el personaje esta bloqueado)
							if ((j == list_size(personaje->recursos)-1) && (personaje->bloqueado==true)) {
								matSolicitud[i][k]+=1;             // Entonces lo pongo en la ubicacion i k de la matriz de solicitud
							} else {
								matAsignacion[i][k]+=1;
							}
						}
					}
				}
			}

			//detectando el interbloqueo
			for (i=0; i<cantPersonajes; i++) {
				fila = 0;
				for (j=0;j<cantRecursos;j++) {
					if ((matSolicitud[i][j]-vecCajas[j].cantidadInstancias) <= 0) {
						fila++;
					}
				}//recDisponibles[j])<=0) fila++;}
				if ((fila == cantRecursos) && (vecSatisfechos[i] != 0)) { //los recursos disponibles satisfacen algun proceso
					vecSatisfechos[i] = 0;
					for (j=0; j<cantRecursos; j++) {
						vecCajas[j].cantidadInstancias +=matAsignacion[i][j];//recDisponibles[j] += matAsignacion[i][j];
					}
				    cantPersonajesSatisfechos++;
				    i = -1;//para volver a recorrer la matriz con los recursos disponibles actualizados
			   	}
			}
			if (cantPersonajesSatisfechos==cantPersonajes) {
				log_debug(logger, "no hay interbloqueo");
				//NO HAY INTERBLOQUEO
			} else {
				log_debug(logger,"hay interbloqueo");
				if (recovery==1) {//notificar a plataforma, entonces vecSatisfechos contendra -1-->el personaje quedo interbloqueado
					encontrado='0';

					while (encontrado == '0') {
						log_debug(logger,"en el while");
						if (vecSatisfechos[i] != 0) {
							//cargar el simbolo de ese personaje
							log_debug(logger,"en el if del while");
							personaje = list_get(list_personajes,i);
							perso = personaje->simbolo;
							log_debug(logger,"en el if del while2");
							encontrado = '1';
						}
						log_debug(logger,"saliendo del if");
					}
					log_debug(logger,"saliendo del while");
					log_debug(logger,"antes de enviar paquete con perso(caja):%c",perso);
					pthread_mutex_lock(&semSockPaq);
					paquete.type = N_MUERTO_POR_DEADLOCK;
					paquete.length = sizeof(tSimbolo);
					paquete.payload[0] = perso;
					enviarPaquete(sockete,&paquete,logger,"enviando notificacion de bloqueo de personajes a plataforma");
					pthread_mutex_unlock(&semSockPaq);
					//eliminar personaje y devolver recursos
					log_debug(logger, "El personaje %c se elimino por participar en un interbloqueo", paquete.payload[0]);
//					pthread_mutex_lock(&semItems);
//					liberarRecsPersonaje2((char) personaje->simbolo); //Libera recursos solo en DESCONEXIN_PERS
//					pthread_mutex_unlock(&semItems);
				}
			}
			pthread_mutex_unlock (&semItems);
			for (i=1;i<cantPersonajes;i++) {//i=1 o 0?
				free(matSolicitud[i]);
				free(matAsignacion[i]);
			}

			free(matAsignacion);
			free(matSolicitud);
			free(vecSatisfechos);
			free(vecCajas);
		}
		nanosleep(&dormir,NULL);
	}
	return 0;
}

void liberarRecsPersonaje(char id){
	pers_t* personaje;

	bool buscarPersonaje(pers_t* perso){return(perso->simbolo==id);}
	//eliminar al personaje de list_personajes y devolverlo para desasignar sus recursos:
	personaje=list_find(list_personajes,(void*)buscarPersonaje);
	list_remove_by_condition(list_personajes,(void*)buscarPersonaje);//elimina el personaje de la lista list_personajes

	void desasignar(char* id1){
		ITEM_NIVEL* itemAux;
		bool buscarRecurso(ITEM_NIVEL* item1){return (item1->id==*id1);}
		itemAux=list_find(list_items,(void*)buscarRecurso);
		itemAux->quantity++;
	}
	list_iterate(personaje->recursos,(void*)desasignar);
	BorrarPersonaje(list_items, id);
	personaje_destroyer(personaje);
}

void actualizaPosicion(int *contMovimiento, int *posX, int *posY) {

	switch (*contMovimiento) {
	case 1:
		(*posX)++; //DERECHA
		break;
	case 2:
		(*posY)--; //ARRIBA
		break;
	case 3:
		(*posX)--; //IZQUIERDA
		break;
	case 4:
		(*posY)++; //ABAJO
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

static void personaje_destroyer(pers_t *personaje) {
	list_destroy_and_destroy_elements(personaje->recursos, free);
	free(personaje);
}
