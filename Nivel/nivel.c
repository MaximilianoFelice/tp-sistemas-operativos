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

t_log *logger;
t_list *list_personajes;
t_list *list_items;
char* nom_nivel;
int cantRecursos;
int maxRows=0, maxCols=0;
tPaquete* paquete;

int cant_enemigos;
int sleep_enemigos;
int hayQueAsesinar = true;
char* dir_plataforma;
int recovery;
tAlgoritmo algoritmo;
int quantum;
uint32_t retardo;
int timeCheck;
char* ip_plataforma;
int port_orq;

int sockete;

pthread_mutex_t semMSJ;
pthread_mutex_t semItems;

int main(int argc, char* argv[]) {

	signal(SIGINT, cerrarForzado);
	char buferNotify[TAM_BUFER];
	int i,vigilante,rv,descriptorNotify;
	struct pollfd uDescriptores[2];
	pers_t pjNew;
	pthread_mutex_init(&semMSJ, NULL );
	pthread_mutex_init(&semItems,NULL);
	int posX = 0, posY = 0;// Para los personajes
	int posRecY = 0, posRecX = 0;// Para los recursos
	extern tPaquete* paquete;
	paquete=malloc(sizeof(tPaquete));

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
	paquete->type=N_HANDSHAKE;
	paquete->length=strlen(nom_nivel)+1;
	memcpy(paquete->payload,nom_nivel,strlen(nom_nivel)+1);
	enviarPaquete(sockete,paquete,logger,"handshake nivel");

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
	paquete->type=N_DATOS;
	paquete->length=sizeof(infoDeNivel);
	memcpy(paquete->payload,&infoDeNivel.delay,sizeof(uint32_t));
	memcpy(paquete->payload+sizeof(uint32_t),&infoDeNivel.quantum,sizeof(int8_t));
	memcpy(paquete->payload+sizeof(uint32_t)+sizeof(int8_t),&infoDeNivel.algoritmo,sizeof(tAlgoritmo));
	enviarPaquete(sockete,paquete,logger,"notificando a plataforma algoritmo");

	//INOTIFY
	descriptorNotify=inotify_init();
	vigilante=inotify_add_watch(descriptorNotify,argv[1],IN_MODIFY);
	if(vigilante==-1) puts("error en inotify add_watch");

	//POLL
	uDescriptores[0].fd=sockete;
	uDescriptores[0].events=POLLIN;
	uDescriptores[1].fd=descriptorNotify;
	uDescriptores[1].events=POLLIN;
	//puts("poll hecho"); //TODO Lo saco porque me dibuja mal despues

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
					pthread_mutex_lock(&semMSJ);
					paquete->type=N_DATOS;
					paquete->length=sizeof(infoDeNivel);
					memcpy(paquete->payload,&infoDeNivel.delay,sizeof(uint32_t));
					memcpy(paquete->payload+sizeof(uint32_t),&infoDeNivel.quantum,sizeof(int8_t));
					memcpy(paquete->payload+sizeof(uint32_t)+sizeof(int8_t),&infoDeNivel.algoritmo,sizeof(tAlgoritmo));
					enviarPaquete(sockete,paquete,logger,"notificando a plataforma algoritmo");
					pthread_mutex_unlock(&semMSJ);
				}
			}
			if(uDescriptores[0].revents & POLLIN){
				recibirPaquete(sockete,&tipoDeMensaje,&payload,logger,"recibiendo mensaje de plataforma");
				int8_t tipoMsj;
				tPregPosicion* posConsultada=malloc(sizeof(tPregPosicion));
				tRtaPosicion posRespondida;
				tMovimientoPers movPersonaje;
				pers_t* personaG=NULL;
				tSimbolo personajeMuerto;
				tipoMsj=(int8_t)tipoDeMensaje;
				ITEM_NIVEL* itemRec;
				switch(tipoMsj){
				case PL_CONEXION_PERS:
					movPersonaje.simbolo=(int8_t)*payload;
					bool buscaPersonaje(pers_t* perso){return (perso->simbolo==movPersonaje.simbolo);}
					personaG=list_find(list_personajes,(void*)buscaPersonaje);
					if(personaG==NULL){
						pjNew.simbolo  = movPersonaje.simbolo;
						pjNew.bloqueado  = false;
						pjNew.recursos = list_create();
						list_add_new(list_personajes,(void*)&pjNew,sizeof(pers_t));
						char symbol=(char) movPersonaje.simbolo;
						pthread_mutex_unlock(&semItems);
						CrearPersonaje(list_items,symbol, INI_X, INI_Y);
						pthread_mutex_unlock(&semItems);
						// Logueo el personaje recien agregado
						log_info(logger, "Se agregó al personaje %c", pjNew.simbolo);
						pthread_mutex_lock(&semMSJ);
						paquete->type=N_CONEXION_EXITOSA;
						paquete->length=0;
						enviarPaquete(sockete,paquete,logger,"notificando a plataforma personaje nuevo aceptado");
						pthread_mutex_unlock(&semMSJ);
					}else {//se encontro=>el personaje ya existe
						pthread_mutex_lock(&semMSJ);
						paquete->type=N_PERSONAJE_YA_EXISTENTE;
						paquete->length=0;
						enviarPaquete(sockete,paquete,logger,"notificando a plataforma personaje ya existente");
						pthread_mutex_unlock(&semMSJ);
					}
				break;
				case PL_MOV_PERSONAJE:
					movPersonaje.simbolo=(int8_t)*payload;
					movPersonaje.direccion=(tDirMovimiento)*(payload+sizeof(int8_t));
					personaG=list_find(list_personajes,(void*)buscaPersonaje);
					if(personaG==NULL){
						pthread_mutex_lock(&semMSJ);
						paquete->type=N_PERSONAJE_INEXISTENTE;
						paquete->length=0;
						enviarPaquete(sockete,paquete,logger,"notificando a plataforma personaje no existe");
						pthread_mutex_unlock(&semMSJ);
					}else{
						char symbol=(char) movPersonaje.simbolo;
						pthread_mutex_lock(&semItems);
						// Busco la posicion actual del personaje
						getPosPersonaje(list_items,symbol, &posX, &posY);
						// calculo el movimiento
						pthread_mutex_unlock(&semItems);
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
						}
						pthread_mutex_lock(&semItems);
						MoverPersonaje(list_items,symbol, posX, posY);
						pthread_mutex_unlock(&semItems);
						pthread_mutex_lock(&semMSJ);
						paquete->type=N_CONFIRMACION_MOV;
						paquete->length=0;
						enviarPaquete(sockete,paquete,logger,"notificando a plataforma personaje movido correctamente");
						pthread_mutex_unlock(&semMSJ);
					}
				break;
				case PL_POS_RECURSO:
					posConsultada->recurso=(tSimbolo)*(payload+sizeof(tSimbolo));//en posConsultada->simbolo viene el simbolo del personaje pero no me interesa
					bool buscarRecurso(ITEM_NIVEL *item){return ((item->id==(char)posConsultada->recurso)&&(item->item_type==RECURSO_ITEM_TYPE));}
					itemRec=list_find(list_items,(void*)buscarRecurso);
					if(itemRec!=NULL){
						posRecX=itemRec->posx;
						posRecY=itemRec->posy;
						posRespondida.posX=posRecX;
						posRespondida.posY=posRecY;
						pthread_mutex_lock(&semMSJ);
						paquete->type=N_POS_RECURSO;
						memcpy(paquete->payload,&posRespondida,sizeof(tRtaPosicion));
						paquete->length=sizeof(tRtaPosicion);
						enviarPaquete(sockete,paquete,logger,"enviando pos de recurso a plataforma");
						pthread_mutex_unlock(&semMSJ);
					}else{
						pthread_mutex_lock(&semMSJ);
						paquete->type=N_RECURSO_INEXISTENTE;
						paquete->length=0;
						enviarPaquete(sockete,paquete,logger,"enviando pos de recurso a plataforma");
						pthread_mutex_unlock(&semMSJ);
					}
				break;
				case PL_SOLICITUD_RECURSO:
					posConsultada->recurso=(tSimbolo)*(payload+sizeof(tSimbolo));
					posConsultada->simbolo=(tSimbolo)*payload;//VER SIEMPRE EN QUE ORDEN LO SERIALIZA
					// Calculo la cantidad de instancias
					int cantInstancias = restarInstanciasRecurso(list_items,posConsultada->recurso);
					if (cantInstancias >= 0) {
						log_info(logger, "Al personaje %c se le dio el recurso %c",posConsultada->simbolo,posConsultada->recurso);
						//Agrego el recurso a la lista de recursos del personaje
						bool buscarPersonaje(pers_t* personaje){return (personaje->simbolo==posConsultada->simbolo);}
						personaG=list_find(list_personajes,(void*)buscarPersonaje);
						list_add_new(personaG->recursos,&(posConsultada->recurso),sizeof(tSimbolo));
						//si estaba bloqueado se desbloquea
						personaG->bloqueado=false;
						pthread_mutex_lock(&semMSJ);
						paquete->type=N_ENTREGA_RECURSO;
						paquete->length=0;
						// Envio mensaje donde confirmo la otorgacion del recurso pedido
						enviarPaquete(sockete,paquete,logger,"enviando confirmacion de otorgamiento de recurso a plataforma");
						pthread_mutex_unlock(&semMSJ);
					} else {
						// Logueo el bloqueo del personaje/
						log_info(logger,"El personaje %c se bloqueo por el recurso %c",posConsultada->simbolo,posConsultada->recurso);
						//se bloquea esperando que le den el recurso
						personaG->bloqueado=true;
						/*pthread_mutex_lock(&semMSJ);
						paquete->type=N_??????????? ;------->FALTA DEFINIR QUE LE MANDO
						paquete->length=0;
						// Envio mensaje donde confirmo la otorgacion del recurso pedido
						enviarPaquete(sockete,paquete,logger,"enviando confirmacion de otorgamiento de recurso a plataforma");
						pthread_mutex_unlock(&semMSJ);*/
					}
				break;
				case PL_DESCONEXION_PERSONAJE://SALIR:// Un personaje termino o murio y debo liberar instancias de recursos que tenia asignado
					//habria un payload con aunque sea el id del personaje, por ahora uso tSimbolo
					memcpy(&personajeMuerto,payload,sizeof(tSimbolo));
					pthread_mutex_lock(&semItems);
					liberarRecsPersonaje(personajeMuerto);
					pthread_mutex_unlock(&semItems);
				break;
				} //Fin del switch
			}
		}
	}
	inotify_rm_watch(descriptorNotify,vigilante);
	close(descriptorNotify);
	return 0;
}

void levantarArchivoConf(char* argumento){
	t_config *configNivel; //TODO destruir el config cuando cierra el nivel,
	char* algoritmoAux;
	int i,posXCaja,posYCaja,cantCajas;
	char* dir_plataforma;
	char* messageLimitErr= malloc(sizeof(char) * 100);
	char** lineaCaja;
	char** dirYpuerto;

	dirYpuerto=(char**)malloc(sizeof(char*)*2);
	dirYpuerto[0]=malloc(sizeof(int)*4+sizeof(char)*3);
	dirYpuerto[1]=malloc(sizeof(int));
	extern char* nom_nivel;

	configNivel=config_create(argumento);
	cantCajas=config_keys_amount(configNivel)-9;
	lineaCaja=malloc(cantCajas*15*sizeof(char));

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
	nom_nivel 	   = string_duplicate(config_get_string_value(configNivel,"Nombre"));//config_get_string_value(configNivel, "Nombre");
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
	ip_plataforma  = string_duplicate(dirYpuerto[0]);//strtok(dir_plataforma, ":");
	port_orq 	   = atoi(dirYpuerto[1]);//strtok(NULL, ":");
	cantRecursos   = list_size(list_items);
	config_destroy(configNivel);
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

	int cantPersonajesActivos;
	int contMovimiento = 3;
	char victimaAsignada='0';
	char ultimoMov='a';
	char dirMov='b';
	int dist1,dist2=9999999,i;
	ITEM_NIVEL* item=malloc(sizeof(ITEM_NIVEL));

	//chequeando que la posicion del enemigo no caiga en un recurso
	enemigo->posX = 1+(rand() % maxCols);
	enemigo->posY = 1+(rand() % maxRows);
	bool esUnRecurso(ITEM_NIVEL *item){ return (item->item_type==RECURSO_ITEM_TYPE&&item->posx==enemigo->posX&&item->posy==enemigo->posY);}
	while(list_any_satisfy(list_items,(void*)esUnRecurso)){
		enemigo->posX=1+(rand() % maxCols);
		enemigo->posY=1+(rand() % maxRows);
	}
	pthread_mutex_lock(&semItems);//????
	CreateEnemy(list_items,enemigo->num_enemy,enemigo->posX,enemigo->posY);
	pthread_mutex_unlock(&semItems);//????

	while (1) {
		//sleep(3);
		//puts("while enemigo");

		bool personajeBloqueado(pers_t* personaje){return(personaje->bloqueado==false);}
		cantPersonajesActivos=list_count_satisfying(list_personajes,(void*)personajeBloqueado);

		if (cantPersonajesActivos == 0) {
			//puts("personajesActivos = ninguno");
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
			void esUnRecurso2(ITEM_NIVEL *item){if (item->item_type==RECURSO_ITEM_TYPE&&item->posx==enemigo->posX&&item->posy==enemigo->posY) enemigo->posX--;}
			list_iterate(list_items,(void*)esUnRecurso2);
		} else { //ELEGIR O PERSEGUIR A LA VICTIMA
			//pers_t* persVictima=malloc(sizeof(pers_t));
			//puts("eligiendo o persiguiendo victima");
			//sleep(1);

			if(victimaAsignada=='0'){     //No tiene victima =>reordenar viendo cual es la victima mas cercana
				pers_t* persVictima1;
				for(i=0;i<cantPersonajesActivos;i++){
					persVictima1=list_get(list_personajes,i);
					//puts("se itero una victima de la lista de list_personajes");
					//printf("esa victima es:%c\n",persVictima1->simbolo);
					if(persVictima1->bloqueado==false){ //el personaje no esta quieto esperando por un recurso
						//puts("y esa victima tiene bloqueado=false");
						pthread_mutex_lock(&semItems);//?????
						bool esElPersonaje(ITEM_NIVEL* personaje){
							char aux =(char)persVictima1->simbolo;
							return(personaje->id==aux);
						}
						item=list_find(list_items,(void*)esElPersonaje);
						pthread_mutex_unlock(&semItems);//?????
						//puts("entonce se la busco en list_items y se cargo en la var item");
						//printf("ese item es:%c\n",item->id);
						dist1=(enemigo->posX-item->posx)*(enemigo->posX-item->posx)+(enemigo->posY-item->posy)*(enemigo->posY-item->posy);
						if(dist1<dist2){
							victimaAsignada=item->id;
							dist2=dist1;
						}
					}
				}
			}else{//ya teiene una victima asignada
				pers_t *persVictima2;
				//puts("ya se le asigno una victma y entramos a la seccion victima asignada");
				bool buscarPersonaje(pers_t* personaje){
					//printf("personaje iterado:%c\n",personaje->simbolo);
					char aux=(char)personaje->simbolo;
					return (aux==item->id);
				}
				persVictima2=list_find(list_personajes,(void*)buscarPersonaje);
				//puts("se busco a la victima cargada en persVictima en list_personajes");
				//printf("y esa victima es:%c y su estado de bloqueo es:%c\n",persVictima2->simbolo,persVictima2->bloqueado);

				if(persVictima2->bloqueado==true){//ver si el personaje que estaba persiguiendo se bloqueo en un recurso=>habra que elegir otra victima
					//puts("se chequeo que la victima esta bloqueada=> buscar otra");
					victimaAsignada='0';
				}else{
					//puts("la victima no estaba bloqueada entonces me fijare");
					//bool unPersonaje(ITEM_NIVEL* item){	return (item->id==victimaAsignada);	}
					//item=list_find(list_personajes,(void*)unPersonaje);
					if(enemigo->posY==item->posy){
						if(enemigo->posX<item->posx){contMovimiento=1;}
						if(enemigo->posX>item->posx){contMovimiento=3;}
						if(enemigo->posX==item->posx){//se esta en la misma posicion que la victima =>matarla
							//un semaforo para que no mande mensaje al mismo tiempo que otros enemigos o el while principal
							//otro semaforo para que no desasigne y se esten evaluando otros
							puts("se mata al perso");
							log_debug(logger, "El personaje %c esta muerto",paquete->type);
							pthread_mutex_lock(&semMSJ);
							paquete->type=N_MUERTO_POR_ENEMIGO;
							memcpy(paquete->payload,&(item->id),sizeof(char));
							paquete->length=sizeof(char);
							enviarPaquete(sockete,paquete,logger,"enviando notificacion de muerte de personaje a plataforma");
							pthread_mutex_unlock(&semMSJ);
							pthread_mutex_lock(&semItems);
							liberarRecsPersonaje(item->id);
							pthread_mutex_unlock(&semItems);
							victimaAsignada='0';
						}
					}else{ //acercarse por fila
						//puts("acercarse por fila");
						if(enemigo->posY<item->posy) contMovimiento=4;
						if(enemigo->posY>item->posy) contMovimiento=2;
				    }
				}
				actualizaPosicion(&contMovimiento, &(enemigo->posX),&(enemigo->posY));
				void esUnRecurso2(ITEM_NIVEL *item){if (item->item_type==RECURSO_ITEM_TYPE&&item->posx==enemigo->posX&&item->posy==enemigo->posY)	enemigo->posX--;}
				list_iterate(list_items,(void*)esUnRecurso2);
			}
			//free(persVictima);
		} //Fin de else
		pthread_mutex_lock(&semItems);
		MoveEnemy(list_items, enemigo->num_enemy, enemigo->posX,enemigo->posY);
		nivel_gui_dibujar(list_items, nom_nivel);
		pthread_mutex_unlock(&semItems);

		usleep(sleep_enemigos);
		} //Fin de while(1)
	free(item);
	pthread_exit(NULL );
}
void *deteccionInterbloqueo (void *parametro){
	t_caja* caja;
	int i,j,k,fila;
	ITEM_NIVEL* item;
	pers_t* personaje;
	t_list* personajesBloqueados;
	struct timespec dormir;
	dormir.tv_sec=(time_t)(timeCheck/1000);
	dormir.tv_nsec=(long)((timeCheck%1000)*1000000);

	while(1){
		pthread_mutex_lock (&semItems);//nadie mueve un pelo hasta que no se evalue el interbloqueo
		int cantPersonajes=list_size(list_personajes);
		int matAsignacion[cantPersonajes][cantRecursos];
		int matSolicitud[cantPersonajes][cantRecursos];
		//int recDisponibles[cantRecursos];
		int vecSatisfechos[cantPersonajes];
		t_caja vecCajas[cantRecursos];//fijarse como hacer t_caja* vecCajas[]=malloc(sizeof(t_caja)*cantRecursos); por que no me deja
		int indice=0,cantPersonajesSatisfechos=0;

		//inicializando matrices
		for(i=0;i<cantRecursos;i++){
			for(j=0;j<cantPersonajes;j++){
				matAsignacion[i][j]=0;
				matSolicitud[i][j]=0;
				//recDisponibles[j]=0;
			}
			vecSatisfechos[i]=-1;
		}
		//llenando las matrices con la info
		for(i=0;i<list_size(list_items);i++){
			item=list_get(list_items,i);
			if(item->item_type==RECURSO_ITEM_TYPE){
				vecCajas[indice].simbolo=item->id;
				vecCajas[indice].cantidadInstancias=item->quantity;
				indice++;
			}
		}
		personajesBloqueados=list_create();
		bool estaBloqueado(pers_t* persoj){return (persoj->bloqueado==true);}
		personajesBloqueados=list_filter(list_personajes,(void*)estaBloqueado);
		for(i=0;i<list_size(personajesBloqueados);i++){
			personaje=list_get(personajesBloqueados,i);
			for(j=0;j<list_size(personaje->recursos);j++){
				caja=list_get(personaje->recursos,j);
				for(k=0;k<cantRecursos;k++){
					if(vecCajas[k].simbolo==caja->simbolo){
						if(j==list_size(personaje->recursos)-1){
							matSolicitud[i][k]+=1;
						}else{
							matAsignacion[i][k]+=1;
						}
					}
				}
			}
		}
		//detectando el interbloqueo
		for(i=0;i<list_size(personajesBloqueados);i++){
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
			//NO HAY INTERBLOQUEO
		}else{
			if(recovery==1){
				//notificar a plataforma, entonces vecSatisfechos contendra -1-->el personaje quedo bloqueado
				pthread_mutex_lock(&semMSJ);
				paquete->type=N_PERSONAJES_DEADLOCK;
				//memcpy(&paquete.payload,item->id,sizeof(char));
				paquete->length=sizeof(char);
				enviarPaquete(sockete,paquete,logger,"enviando notificacion de bloqueo de personajes a plataforma");
				/*
				msj.type=NIVEL;
				msj.detail=INFO;
				msj.detail2=list_size(personajesBloqueados)-cantPersonajesSatisfechos;
				enviaMensaje(sockPlanif, &msj, sizeof(msj), logger,	"Enviando aviso de bloqueo");*/
				/*
				for(i=0;i<list_size(personajesBloqueados)-cantPersonajesSatisfechos;i++){
					//ESPERAR CONTESTACION???
					personaje=list_get(personajesBloqueados,i);
					if(vecSatisfechos[i]==-1){
						msj.name=personaje->simbolo;
						enviaMensaje(sockPlanif, &msj, sizeof(msj), logger,	"Enviando el id del personaje bloqueado");
					}
				}*/
				pthread_mutex_unlock(&semMSJ);
			}
		}
		pthread_mutex_unlock (&semItems);
		nanosleep(&dormir,NULL);
	}
	return 0;
}
void liberarRecsPersonaje(char id){
	pers_t* personaje;

	bool buscarPersonaje(pers_t* perso){return(perso->simbolo==id);}
	//eliminar al personaje de list_personajes y devolverlo para desasignar sus recursos:
	personaje=list_find(list_personajes,(void*)buscarPersonaje);
	list_remove_by_condition(list_personajes,(void*)buscarPersonaje);

	void desasignar(char id1){
		ITEM_NIVEL* itemAux;
		bool buscarRecurso(ITEM_NIVEL* item1){return (item1->id==id1);}
		itemAux=list_find(list_items,(void*)buscarRecurso);
		itemAux->quantity++;
	}
	list_iterate(personaje->recursos,(void*)desasignar);
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
/*
static void recurso_destroyer(char *recurso) {
	free(recurso);
}*/
