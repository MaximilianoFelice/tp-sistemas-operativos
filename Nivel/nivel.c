/*
 * Sistemas Operativos - Super Mario Proc RELOADED.
 * Grupo       : C o no ser.
 * Nombre      : nivel.c.
 * Descripcion : Este archivo contiene la implementacion de las
 * funciones usadas por el nivel.
 */

#include "nivel.h"
#define TAM_EVENTO (sizeof(struct inotify_event)+24)
#define TAM_BUFER (1*TAM_EVENTO)//ex 1024

t_log *logger=NULL;
t_list *list_personajes=NULL;
t_list *list_items=NULL;
char* nom_nivel=NULL;
int cantRecursos;
int maxRows=0, maxCols=0;

//Variables del archivo config
int cant_enemigos;
int sleep_enemigos;
int hayQueAsesinar = true;
int recovery;
tAlgoritmo algoritmo;
int quantum;
uint32_t retardo;
int timeCheck;
char* dir_plataforma=NULL;
char* ip_plataforma=NULL;
int port_orq;

int sockete;
tPaquete paquete;

pthread_mutex_t semSockPaq;
pthread_mutex_t semItems;

int main(int argc, char* argv[]) {
	extern char* nom_nivel;
	extern tPaquete paquete;

	pers_t* pjNew=(pers_t*)malloc(sizeof(pers_t));

	signal(SIGINT, cerrarForzado);
	char buferNotify[TAM_BUFER];
	int i,vigilante,rv,descriptorNotify;
	struct pollfd uDescriptores[2];
	pthread_mutex_init(&semSockPaq, NULL );
	pthread_mutex_init(&semItems,NULL);
	int posX = 0, posY = 0;// Para los personajes
	int posRecY = 0, posRecX = 0;// Para los recursos

	int8_t tipoMsj;
	tRtaPosicion posRespondida;
	tMovimientoPers movPersonaje;

	//LOG
	logger = logInit(argv, "NIVEL");

	// INICIALIZANDO GRAFICA DE MAPAS
	list_items = list_create();
	list_personajes = list_create();
	nivel_gui_inicializar();
	nivel_gui_get_area_nivel(&maxRows, &maxCols);

	//LEVANTAR EL ARCHIVO CONFIGURACION EN VARIABLES GLOBALES
	levantarArchivoConf(argv[1]);

	//SOCKETS
	sockete=connectToServer(ip_plataforma,port_orq,logger);
    if (sockete == EXIT_FAILURE) {
            nivel_gui_terminar();
            exit(EXIT_FAILURE);
    }

	// MENSAJE INICIAL A ORQUESTADOR (SALUDO)
	//la serializacion:
	paquete.type=N_HANDSHAKE;
	paquete.length=strlen(nom_nivel)+1;
	memcpy(paquete.payload,nom_nivel,strlen(nom_nivel)+1);
	enviarPaquete(sockete,&paquete,logger,"handshake nivel");

	//RECIBIENDO CONTESTACION
	tMensaje tipoDeMensaje;
	char* payload;
	recibirPaquete(sockete,&tipoDeMensaje,&payload,logger,"recibe handshake de plataforma");
	if(tipoDeMensaje==PL_NIVEL_YA_EXISTENTE){
		log_error(logger,"ya existe nivel con ese nombre");
		exit(EXIT_FAILURE);
	}else{
		if(tipoDeMensaje!=PL_HANDSHAKE) {
			log_error(logger,"tipo de mensaje incorrecto -se esperaba PL_HANDSHAKE-");
			exit(EXIT_FAILURE);
		}
	}

	//ENVIANDO A PLATAFORMA NOTIFICACION DE ALGORITMO ASIGNADO
	tInfoNivel infoDeNivel;
	tipoDeMensaje=N_DATOS;
	infoDeNivel.algoritmo=algoritmo;
	infoDeNivel.quantum=quantum;
	infoDeNivel.delay=retardo;
	//serializacion propia porque la de protocolo no me andaba bien
	paquete.type=N_DATOS;
	paquete.length=sizeof(infoDeNivel);
	memcpy(paquete.payload,&infoDeNivel.delay,sizeof(uint32_t));
	memcpy(paquete.payload+sizeof(uint32_t),&infoDeNivel.quantum,sizeof(int8_t));
	memcpy(paquete.payload+sizeof(uint32_t)+sizeof(int8_t),&infoDeNivel.algoritmo,sizeof(tAlgoritmo));
	enviarPaquete(sockete,&paquete,logger,"notificando a plataforma algoritmo");
	//free(nom_nivel);--->NO, la usaran los hilos enemigos
	free(ip_plataforma);
	free(dir_plataforma);

	//INOTIFY
	descriptorNotify=inotify_init();
	vigilante=inotify_add_watch(descriptorNotify,argv[1],IN_MODIFY);
	if(vigilante==-1) puts("error en inotify add_watch");

	//POLL
	uDescriptores[0].fd=sockete;
	uDescriptores[0].events=POLLIN;
	uDescriptores[1].fd=descriptorNotify;
	uDescriptores[1].events=POLLIN;

	////CREANDO Y LANZANDO HILOS ENEMIGOS
	threadEnemy_t *hilosEnemigos;
	hilosEnemigos = calloc(cant_enemigos, sizeof(threadEnemy_t));
	for (i = 0; i < cant_enemigos; i++) {
		hilosEnemigos[i].enemy.num_enemy = i + 1; //El numero o id de enemigo
		hilosEnemigos[i].enemy.sockP = sockete;
		if (pthread_create(&hilosEnemigos[i].thread_enemy, NULL, enemigo,(void*) &hilosEnemigos[i].enemy)) {
			log_error(logger, "pthread_create: %s", strerror(errno));
			exit(EXIT_FAILURE);
		}
	}

	//LANZANDO EL HILO DETECTOR DE INTERBLOQUEO
	pthread_t hiloInterbloqueo;
	pthread_create(&hiloInterbloqueo,NULL,&deteccionInterbloqueo,NULL);

	//WHILE PRINCIPAL
	while (1) {
		if((rv=poll(uDescriptores,2,-1))==-1) perror("poll");
		else{
			if (uDescriptores[1].revents&POLLIN){
				puts("novedad es POLLIN");
				read(descriptorNotify,buferNotify,TAM_BUFER);
				puts("se lee el descriptor en el buffer");
				struct inotify_event* evento=(struct inotify_event*) &buferNotify[0];
				if(evento->mask & IN_MODIFY){//avisar a planificador que cambio el archivo config
					actualizarInfoNivel(argv[1]);
					//ENVIANDO A PLATAFORMA NOTIFICACION DE ALGORITMO ASIGNADO
					tInfoNivel infoDeNivel;
					infoDeNivel.algoritmo=algoritmo;
					infoDeNivel.quantum=quantum;
					infoDeNivel.delay=retardo;
					//serializacion propia porque la de protocolo no me andaba bien
					pthread_mutex_lock(&semSockPaq);
					paquete.type=N_DATOS;
					paquete.length=sizeof(infoDeNivel);
					memcpy(paquete.payload,&infoDeNivel.delay,sizeof(uint32_t));
					memcpy(paquete.payload+sizeof(uint32_t),&infoDeNivel.quantum,sizeof(int8_t));
					memcpy(paquete.payload+sizeof(uint32_t)+sizeof(int8_t),&infoDeNivel.algoritmo,sizeof(tAlgoritmo));
					enviarPaquete(sockete,&paquete,logger,"notificando a plataforma algoritmo");
					pthread_mutex_unlock(&semSockPaq);
				}
			}
			if(uDescriptores[0].revents & POLLIN){
				tPregPosicion* posConsultada=NULL;
				pers_t* personaG=NULL;
				ITEM_NIVEL* itemRec=NULL;
				tDesconexionPers* persDesconectado;
				recibirPaquete(sockete,&tipoDeMensaje,&payload,logger,"recibiendo mensaje de plataforma");

//				tRtaPosicion2 posRta; //No borres lee el comentario del case PL_POS_RECURSO
				tMovimientoPers movPersonaje;
				tipoMsj=(int8_t)tipoDeMensaje;

				switch(tipoMsj){
				case PL_CONEXION_PERS:
					movPersonaje.simbolo=(int8_t)*payload;
					bool buscaPersonaje(pers_t* perso){return (perso->simbolo==movPersonaje.simbolo);}
					personaG=list_find(list_personajes,(void*)buscaPersonaje);
					if(personaG==NULL){
						pjNew->simbolo  = movPersonaje.simbolo;
						pjNew->bloqueado  = false;
						pjNew->recursos = list_create();
						list_add_new(list_personajes,(void*)pjNew,sizeof(pers_t));
						char symbol=(char) movPersonaje.simbolo;
						pthread_mutex_unlock(&semItems);
						CrearPersonaje(list_items,symbol, INI_X, INI_Y);
						pthread_mutex_unlock(&semItems);
						// Logueo el personaje recien agregado
						log_info(logger, "<<< Se agrego al personaje %c a la lista", (char) pjNew->simbolo);
						pthread_mutex_lock(&semSockPaq);
						paquete.type=N_CONEXION_EXITOSA;
						paquete.length=0;
						enviarPaquete(sockete,&paquete,logger,"notificando a plataforma personaje nuevo aceptado");
						pthread_mutex_unlock(&semSockPaq);
					}else {//se encontro=>el personaje ya existe
						pthread_mutex_lock(&semSockPaq);
						paquete.type=N_PERSONAJE_YA_EXISTENTE;
						paquete.length=0;
						enviarPaquete(sockete,&paquete,logger,"notificando a plataforma personaje ya existente");
						pthread_mutex_unlock(&semSockPaq);
					}
				break;
				case PL_MOV_PERSONAJE:
					movPersonaje.simbolo=(int8_t)*payload;
					movPersonaje.direccion=(tDirMovimiento)*(payload+sizeof(int8_t));
					log_debug(logger, "<<< El personaje %c solicito moverse", movPersonaje.simbolo);
					bool buscaPer(pers_t* perso){
						log_debug(logger,"perso.simbolo:%c movPerso.simbolo:%c",perso->simbolo,movPersonaje.simbolo);
						return (perso->simbolo==movPersonaje.simbolo);
					}
					personaG=(pers_t *)list_find(list_personajes,(void*)buscaPer);
					if(personaG==NULL){
						pthread_mutex_lock(&semSockPaq);
						paquete.type=N_PERSONAJE_INEXISTENTE;
						paquete.length=0;
						enviarPaquete(sockete,&paquete,logger,"notificando a plataforma personaje no existe");
						pthread_mutex_unlock(&semSockPaq);
					}else{
						personaG->bloqueado=false;//si era true se pone en false porque planificador lo desbloqueo asignandole un recurso---->ULTIMA DESICION
						char symbol=(char) movPersonaje.simbolo;
						// Busco la posicion actual del personaje
						getPosPersonaje(list_items,symbol, &posX, &posY);
						log_debug(logger, "Posicion actual del personaje %c: (%d,%d)", symbol, posX, posY);
						// calculo el movimiento
						switch (movPersonaje.direccion) {
							case arriba:
								if (posY > 1) posY--;
							break;
							case abajo:
								if (posY < maxRows) posY++;
							break;
							case izquierda:
								if (posX > 1) posX--;
							break;
							case derecha:
								if (posX < maxCols) posX++;
							break;
							default:
								log_error(logger, "ERROR: no detecto una direccion de movimiento valida: %d", movPersonaje.direccion);
							break;
						}
					pthread_mutex_lock(&semItems);
					MoverPersonaje(list_items,symbol, posX, posY);
					pthread_mutex_unlock(&semItems);
					pthread_mutex_lock(&semSockPaq);
					paquete.type = N_CONFIRMACION_MOV;
					paquete.length = 0;
					enviarPaquete(sockete,&paquete,logger,"notificando a plataforma personaje movido correctamente");
					pthread_mutex_unlock(&semSockPaq);
					}
				break;
				case PL_POS_RECURSO:
					posConsultada = deserializarPregPosicion(payload);
					log_debug(logger, "<<< Personaje %c solicita la posicion del recurso %c", (char)posConsultada->simbolo, (char)posConsultada->recurso);
					bool buscarRecurso(ITEM_NIVEL *item){return ((item->id==(char)posConsultada->recurso)&&(item->item_type==RECURSO_ITEM_TYPE));}
					itemRec=list_find(list_items,(void*)buscarRecurso);
					//Cesar no borres esto lo deje porque capaz necesite este mensaje para la planificacion SRDF
					//Donde ademas de la posicion del recurso le paso tambien la distancia del personaje al recurso
					//Ademas usa un tRtaPosicion2 con sus correspondientes serializadores y deserializadores
//					if(itemRec!=NULL){
//						posRecX=itemRec->posx;
//						posRecY=itemRec->posy;
//						posRta.posX=posRecX;
//						posRta.posY=posRecY;
//						int posPerX, posPerY;
//						getPosPersonaje(list_items, posConsultada->simbolo, &posPerX, &posPerY);
//						posRta.RD = distancia(posPerX, posPerY, posRecX, posRecY);
//						pthread_mutex_lock(&semMSJ);
//						serializarRtaPosicion2(N_POS_RECURSO, posRta, paquete);
//						enviarPaquete(sockete,paquete,logger,"enviando pos de recurso a plataforma");
//						pthread_mutex_unlock(&semMSJ);
//					}
					if(itemRec!=NULL){
						posRecX=itemRec->posx;
						posRecY=itemRec->posy;
						posRespondida.posX=posRecX;
						posRespondida.posY=posRecY;
						pthread_mutex_lock(&semSockPaq);
						paquete.type=N_POS_RECURSO;
						memcpy(paquete.payload,&posRespondida,sizeof(tRtaPosicion));
						paquete.length=sizeof(tRtaPosicion);
						enviarPaquete(sockete,&paquete,logger,"enviando pos de recurso a plataforma");
						pthread_mutex_unlock(&semSockPaq);
					}else{
						pthread_mutex_lock(&semSockPaq);
						paquete.type=N_RECURSO_INEXISTENTE;
						paquete.length=0;
						enviarPaquete(sockete,&paquete,logger,"el recurso solicitado no existe");
						pthread_mutex_unlock(&semSockPaq);
					}
				break;
				case PL_SOLICITUD_RECURSO:
					posConsultada=deserializarPregPosicion(payload);
					log_debug(logger, "<<< Personaje %c solicita una instancia del recurso %c", (char)posConsultada->simbolo, (char)posConsultada->recurso);
					// Calculo la cantidad de instancias
					pthread_mutex_lock(&semItems);
					int cantInstancias = restarInstanciasRecurso(list_items,posConsultada->recurso);
					pthread_mutex_unlock(&semItems);
					if (cantInstancias >= 0) {
						log_info(logger, "Al personaje %c se le dio el recurso %c",posConsultada->simbolo,posConsultada->recurso);
						bool buscarPersonaje(pers_t* personaje){return (personaje->simbolo==posConsultada->simbolo);}
						personaG=list_find(list_personajes,(void*)buscarPersonaje);
						//Agrego el recurso a la lista de recursos del personaje y lo desbloquea si estaba bloqueado
						void agregaRecursoYdesboquea(pers_t *personaje){
							if(personaje->simbolo==personaG->simbolo){
								personaje->bloqueado=false;
								list_add_new(personaje->recursos,&(posConsultada->recurso),sizeof(tSimbolo));
							}
						}
						list_iterate(list_personajes,(void*) agregaRecursoYdesboquea);
						pthread_mutex_lock(&semSockPaq);
						paquete.type=N_ENTREGA_RECURSO;
						paquete.length=0;
						// Envio mensaje donde confirmo la otorgacion del recurso pedido
						enviarPaquete(sockete,&paquete,logger,"enviando confirmacion de otorgamiento de recurso a plataforma");
						pthread_mutex_unlock(&semSockPaq);
					} else {
						bool buscPers(pers_t* personaje){return (personaje->simbolo==posConsultada->simbolo);}
						personaG=list_find(list_personajes,(void*)buscPers);
						personaG->recursos=list_create();
						// Logueo el bloqueo del personaje/
						log_info(logger,"El personaje %c se bloqueo por el recurso %c",posConsultada->simbolo,posConsultada->recurso);
						//Agrego el recurso a la lista de recursos del personaje y lo bloqueo
						void agregaRecursoYbloquea(pers_t *personaje){
							if(personaje->simbolo==personaG->simbolo){
								personaje->bloqueado=true;
								list_add_new(personaje->recursos,&(posConsultada->recurso),sizeof(tSimbolo));
							}
						}
						list_iterate(list_personajes,(void*)agregaRecursoYbloquea);

						/*pthread_mutex_lock(&semMSJ);
						paquete->type=N_??????????? ;------->FALTA DEFINIR QUE LE MANDO
						paquete->length=0;
						// Envio mensaje donde confirmo la otorgacion del recurso pedido
						enviarPaquete(sockete,paquete,logger,"enviando confirmacion de otorgamiento de recurso a plataforma");
						pthread_mutex_unlock(&semMSJ);*/
					}
				break;
				case PL_DESCONEXION_PERSONAJE:// Un personaje termino o murio y debo liberar instancias de recursos que tenia asignado
					persDesconectado= deserializarPersDesconect(payload);
					log_debug(logger, "El personaje %c se desconecto",persDesconectado->simbolo);
					pthread_mutex_lock(&semItems);
					//agrego una instancia a list_items de todos los recursos que me manda planificador (que son todos los que no reasigno)
					for(i=0;i<persDesconectado->lenghtRecursos;i++){
						sumarRecurso(list_items,persDesconectado->recursos[i]);
					}
					pthread_mutex_unlock(&semItems);
					//eliminar al personaje de list_personajes
					bool buscarPersonaje(pers_t* perso){return(perso->simbolo==persDesconectado->simbolo);}
					list_remove_by_condition(list_personajes,(void*)buscarPersonaje);
				break;
				} //Fin del switch
				//Cesar lo pongo que dibuje porque estuve probando que los enemigos son MUY LENTOS entonces no llegarian nunca a dibujar
				//Entonces lo agrego aqui para que dibuje el nivel también.
				//Ademas sería una posibilidad factible que nos hagan probar enemigos muy lentos en el examen
				nivel_gui_dibujar(list_items, nom_nivel);
			}
		}
	}
	inotify_rm_watch(descriptorNotify,vigilante);
	close(descriptorNotify);
	return 0;
}

void levantarArchivoConf(char* argumento){
	t_config *configNivel; //TODO destruir el config cuando cierra el nivel,
	extern char* dir_plataforma;
	extern char* ip_plataforma;
	dir_plataforma=(char*)malloc(sizeof(char)*22);
	ip_plataforma=(char*)malloc(sizeof(char)*16);
	char* algoritmoAux;
	int i,posXCaja,posYCaja,cantCajas;
	char* messageLimitErr= malloc(sizeof(char) * 100);
	char** lineaCaja;
	char** dirYpuerto;

	dirYpuerto=(char**)malloc(sizeof(char*)*2);
	dirYpuerto[0]=malloc(sizeof(int)*4+sizeof(char)*3);
	dirYpuerto[1]=malloc(sizeof(int));
	extern char* nom_nivel;

	configNivel=config_create(argumento);
	cantCajas=config_keys_amount(configNivel)-9;


	for(i=0;i<cantCajas;i++){
		//printf("valor de i:%i",i);
		char* clave;
		clave=string_from_format("Caja%i",i+1);
		lineaCaja=config_get_array_value(configNivel,clave);
	    posXCaja=atoi(lineaCaja[3]);
		posYCaja=atoi(lineaCaja[4]);
		if (posYCaja > maxRows || posXCaja > maxCols || posYCaja < 1 || posXCaja < 1) {
			sprintf(messageLimitErr, "La caja %s excede los limites de la pantalla. (%d,%d) - (%d,%d)",clave,posXCaja,posYCaja,maxRows,maxCols);
			cerrarNivel(messageLimitErr);
			exit(EXIT_FAILURE);
		}
		pthread_mutex_lock(&semItems);
		// Si la validacion fue exitosa creamos la caja de recursos
		CrearCaja(list_items, *lineaCaja[1],atoi(lineaCaja[3]),atoi(lineaCaja[4]),atoi(lineaCaja[2]));//*cajaRecursos[1], atoi(cajaRecursos[3]), atoi(cajaRecursos[4]), atoi(cajaRecursos[2]));
		pthread_mutex_unlock(&semItems);
	}
	free(lineaCaja);
	free(messageLimitErr);
	nom_nivel 	   = string_duplicate(config_get_string_value(configNivel,"Nombre"));
	recovery       = config_get_int_value(configNivel, "Recovery");
	cant_enemigos  = config_get_int_value(configNivel, "Enemigos");
	sleep_enemigos = config_get_int_value(configNivel, "Sleep_Enemigos");
	algoritmoAux   = config_get_string_value(configNivel, "algoritmo");
	char* rr="RR";
	if(strcmp(algoritmoAux,rr)==0)algoritmo=RR;else algoritmo=SRDF;
	quantum 	   = config_get_int_value(configNivel, "quantum");
	retardo 	   = config_get_int_value(configNivel, "retardo");
	timeCheck      = config_get_int_value(configNivel, "TiempoChequeoDeadlock");
	dir_plataforma = config_get_string_value(configNivel, "Plataforma");
	dirYpuerto     = string_split(dir_plataforma,":");
	ip_plataforma  = string_duplicate(dirYpuerto[0]);
	port_orq 	   = atoi(dirYpuerto[1]);
	cantRecursos   = list_size(list_items);

	//config_destroy(configNivel);
	free(dirYpuerto);
}
void actualizarInfoNivel(char* argumento){
	extern tAlgoritmo algoritmo;
	extern int quantum;
	extern uint32_t retardo;
	char* algoritmoAux;
	t_config *configNivel;

	configNivel=config_create(argumento);
	puts("se hizo configCreate");
	algoritmoAux   = config_get_string_value(configNivel, "algoritmo");
	printf("se levanto algoritmo con:%s\n",algoritmoAux);
	char rr[]="RR";
	if(strcmp(algoritmoAux,rr)==0)
		algoritmo=RR;else algoritmo=SRDF;
	quantum 	   = config_get_int_value(configNivel, "quantum");
	retardo 	   = config_get_int_value(configNivel, "retardo");
	//config_destroy(configNivel);
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
	pthread_mutex_lock(&semItems);//????
	CreateEnemy(list_items,enemigo->num_enemy,enemigo->posX,enemigo->posY);
	pthread_mutex_unlock(&semItems);//????

	while (1) {
		bool personajeBloqueado(pers_t* personaje){return(personaje->bloqueado==false);}
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
		} else { //ELEGIR O PERSEGUIR A LA VICTIMA
			if(victimaAsignada=='0'){//No tiene victima =>reordenar viendo cual es la victima mas cercana
				//haciendo esta busqueda mas rudimentaria porque no se por que valgrind me tiraba que estaba leyendo 1-4 bytes de mas cuando accedia a itemBusq->algo
				for(i=0;i<list_size(list_items);i++){
					item=list_get(list_items,i);
					dist1=(enemigo->posX-item->posx)*(enemigo->posX-item->posx)+(enemigo->posY-item->posy)*(enemigo->posY-item->posy);
					if((dist1<dist2)&&(item->item_type==PERSONAJE_ITEM_TYPE)){
						victimaAsignada=item->id;
						dist2=dist1;
					}
				}
			}else{//ya teiene una victima asignada=>busco la victimaAsignada en la lista de items y la coloco en persVictima
				//Haciendo la busqueda a manopla por que lo de arriba tira en valgrind que se accede a un byte no reservado
				int j=0;
				for(i=0;i<list_size(list_personajes);i++){
					persVictima=list_get(list_personajes,i);
					if((char)persVictima->simbolo==victimaAsignada) j=i;
				}
				persVictima=list_get(list_personajes,j);
				if(persVictima->bloqueado==true){//ver si el personaje que estaba persiguiendo se bloqueo en un recurso=>habra que elegir otra victima
					victimaAsignada='0';
				}else{
					//ITEM_NIVEL* item=malloc(sizeof(ITEM_NIVEL));
					/*bool esElPersonaje(ITEM_NIVEL* personaje){
						return(personaje->id==victimaAsignada);
					}
					item=list_find(list_items,(void*)esElPersonaje);*/
					//cambie de la manera de arriba a esta por lo mismo de antes
					j=0;
					for(i=0;i<list_size(list_items);i++){
						item=list_get(list_items,i);
						if(item->id==victimaAsignada) j=i;
					}
					item=list_get(list_items,j);

					if(enemigo->posY==item->posy){
						if(enemigo->posX<item->posx){contMovimiento=1;}
						if(enemigo->posX>item->posx){contMovimiento=3;}
						if(enemigo->posX==item->posx){//se esta en la misma posicion que la victima =>matarla
							//un semaforo para que no mande mensaje al mismo tiempo que otros enemigos o el while principal
							//otro semaforo para que no desasigne y se esten evaluando otros

							pthread_mutex_lock(&semSockPaq);
							paquete.type=N_MUERTO_POR_ENEMIGO;
							memcpy(paquete.payload,&(item->id),sizeof(char));
							paquete.length=sizeof(char);
							enviarPaquete(sockete,&paquete,logger,"enviando notificacion de muerte de personaje a plataforma");
							pthread_mutex_unlock(&semSockPaq);
							pthread_mutex_lock(&semItems);
							liberarRecsPersonaje(item->id);
							pthread_mutex_unlock(&semItems);
							victimaAsignada='0';
						}
					}else{ //acercarse por fila
						if(enemigo->posY<item->posy) contMovimiento=4;
						if(enemigo->posY>item->posy) contMovimiento=2;
				    }
				}
				actualizaPosicion(&contMovimiento, &(enemigo->posX),&(enemigo->posY));
				void esUnRec(ITEM_NIVEL *iten){
					if ((iten->item_type==RECURSO_ITEM_TYPE)&&((iten->posx==enemigo->posX)&&(iten->posy==enemigo->posY))) enemigo->posX--;
				}
				list_iterate(list_items,(void*)esUnRec);
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
void *deteccionInterbloqueo (void *parametro){
	extern t_list* list_items;
	extern tPaquete paquete;

	tSimbolo* caja;
	tSimbolo perso;

	int i,j,k,fila;
	ITEM_NIVEL* itemRec=NULL;
	pers_t* personaje=NULL;
	struct timespec dormir;
	dormir.tv_sec=(time_t)(timeCheck/1000);
	dormir.tv_nsec=(long)((timeCheck%1000)*1000000);

	int cantPersonajes;
	int **matAsignacion=NULL;
	int **matSolicitud=NULL;
	int *vecSatisfechos=NULL;
	t_caja *vecCajas=NULL;
	int indice,cantPersonajesSatisfechos;
	char encontrado;

	while(1){
	    cantPersonajes=list_size(list_personajes);
		indice=0;
		cantPersonajesSatisfechos=0;

		if(cantPersonajes!=0){
			matAsignacion=(int**)malloc(sizeof(int*)*cantPersonajes);
			matSolicitud=(int**)malloc(sizeof(int*)*cantPersonajes);
			vecSatisfechos=(int*)malloc(sizeof(int)*cantPersonajes);
			vecCajas=(t_caja*)malloc(sizeof(t_caja)*cantRecursos);

			for(i=0;i<cantPersonajes;i++){
				matAsignacion[i]=(int*)malloc(sizeof(int)*cantRecursos);
				matSolicitud[i]=(int*)malloc(sizeof(int)*cantRecursos);
			}

		    pthread_mutex_lock (&semItems);//nadie mueve un pelo hasta que no se evalue el interbloqueo
			//inicializando matrices
			for(i=0;i<cantPersonajes;i++){
				for(j=0;j<cantRecursos;j++){
					matAsignacion[i][j]=0;
					*(*(matSolicitud+i)+j)=0;
				}
				*(vecSatisfechos+i)=-1;
			}
			//llenando las matrices con la info
			for(i=0;i<list_size(list_items);i++){
				itemRec=list_get(list_items,i);
				if(itemRec->item_type==RECURSO_ITEM_TYPE){
					vecCajas[indice].simbolo=itemRec->id;
					vecCajas[indice].cantidadInstancias=itemRec->quantity;
					indice++;
				}
			}
			for(i=0;i<cantPersonajes;i++){//recorro la lista de personajes y por cada personaje
				personaje=list_get(list_personajes,i);
				for(j=0;j<list_size(personaje->recursos);j++){       //recorro su lista de recursos y por cada recurso asignados:
					caja=list_get(personaje->recursos,j);
					for(k=0;k<cantRecursos;k++){                     //recorro a vecCajas (vector de recursos en list_items) viendo por c/u de sus elementos
						if(vecCajas[k].simbolo==(char)*caja){        //si coincide con el recurso recorrido con j (esto es para que las matrices mantengan los indices)
							if((j==list_size(personaje->recursos)-1)&&(personaje->bloqueado==true)){//me fijo si es el ultimo elemento de la lista(=>es el por el que el personaje esta bloqueado)
								matSolicitud[i][k]+=1;               //entonces lo pongo en la ubicacion i k de la matriz de solicitud
							}else{
								matAsignacion[i][k]+=1;
							}
						}
					}
				}
			}

			//detectando el interbloqueo
			for(i=0;i<cantPersonajes;i++){
				fila=0;
				for(j=0;j<cantRecursos;j++){ if((matSolicitud[i][j]-vecCajas[j].cantidadInstancias)<=0) fila++;}//recDisponibles[j])<=0) fila++;}
				if((fila==cantRecursos)&&(vecSatisfechos[i]!=0)){ //los recursos disponibles satisfacen algun proceso
					vecSatisfechos[i]=0;
					for(j=0;j<cantRecursos;j++){
						vecCajas[j].cantidadInstancias +=matAsignacion[i][j];//recDisponibles[j] += matAsignacion[i][j];
					}
				    cantPersonajesSatisfechos++;
				    i=-1;//para volver a recorrer la matriz con los recursos disponibles actualizados
			   	}
			}
			if(cantPersonajesSatisfechos==cantPersonajes){
				log_debug(logger, "no hay interbloqueo");
				//NO HAY INTERBLOQUEO
			}else{
				log_debug(logger,"hay interbloqueo");
				if(recovery==1){//notificar a plataforma, entonces vecSatisfechos contendra -1-->el personaje quedo interbloqueado
					encontrado='0';
					while(encontrado=='0'){
						log_debug(logger,"en el while");
						if(vecSatisfechos[i]!=0){
							//cargar el simbolo de ese personaje
							log_debug(logger,"en el if del while");
							personaje=list_get(list_personajes,i);
							perso=personaje->simbolo;
							log_debug(logger,"en el if del while2");
							encontrado='1';
						}
						log_debug(logger,"saliendo del if");
					}
					log_debug(logger,"saliendo del while");
					log_debug(logger,"antes de enviar paquete con perso(caja):%c",perso);
					pthread_mutex_lock(&semSockPaq);
					paquete.type=N_MUERTO_POR_DEADLOCK;
					paquete.length=sizeof(tSimbolo);
					paquete.payload[0]=perso;
					enviarPaquete(sockete,&paquete,logger,"enviando notificacion de bloqueo de personajes a plataforma");
					pthread_mutex_unlock(&semSockPaq);
					//eliminar personaje y devolver recursos
					log_debug(logger, "El personaje %c se elimino por participar en un interbloqueo", paquete.payload[0]);
					pthread_mutex_lock(&semItems);
					liberarRecsPersonaje2((char) personaje->simbolo);
					pthread_mutex_unlock(&semItems);
				}
			}
			pthread_mutex_unlock (&semItems);
			for(i=1;i<cantPersonajes;i++){//i=1 o 0?
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
void liberarRecsPersonaje2(char id){
	pers_t *persDesconexion;

	bool buscarPersonaje(pers_t* perso){return(perso->simbolo==id);}
	//eliminar al personaje de list_personajes y devolverlo para desasignar sus recursos:
	persDesconexion=list_find(list_personajes,(void*)buscarPersonaje);
	list_remove_by_condition(list_personajes,(void*)buscarPersonaje);
	log_debug(logger, "Pase el list_remove del personaje %c", persDesconexion->simbolo);
	int i;
	for(i=0; i<list_size(persDesconexion->recursos); i++){
		tSimbolo *recurso = list_get(persDesconexion->recursos, i);
		sumarInstanciasRecurso(list_items, *recurso);
	}
	list_destroy_and_destroy_elements(persDesconexion->recursos, free);
	BorrarItem(list_items, id);
	personaje_destroyer(persDesconexion);
}
int distancia(int posXPer, int posYPer, int posX, int posY){
	 return pow((posXPer-posX),2) + pow((posYPer-posY), 2);
}
void liberarRecsPersonaje(char id){
	pers_t* personaje;

	bool buscarPersonaje(pers_t* perso){return(perso->simbolo==id);}
	//eliminar al personaje de list_personajes y devolverlo para desasignar sus recursos:
	personaje=list_find(list_personajes,(void*)buscarPersonaje);
	list_remove_by_condition(list_personajes,(void*)buscarPersonaje);
	log_debug(logger, "Pase el list_remove");

	void desasignar(char id1){
		ITEM_NIVEL* itemAux;
		bool buscarRecurso(ITEM_NIVEL* item1){return (item1->id==id1);}
		itemAux=list_find(list_items,(void*)buscarRecurso);
		itemAux->quantity++;
	}
	list_iterate(personaje->recursos,(void*)desasignar);
	log_debug(logger, "Pase el list_iterate()");
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
// Libera memoria de cada personaje de la lista
static void personaje_destroyer(pers_t *personaje) {
	free(personaje);
}
