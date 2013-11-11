/*
 * Sistemas Operativos - Super Mario Proc RELOADED.
 * Grupo       : C o no ser.
 * Nombre      : nivel.c.
 * Descripcion : Este archivo contiene la implementacion de las
 * funciones usadas por el nivel.
 */

#include "nivel.h"
#define TAM_EVENTO (sizeof(struct inotify_event)+24)
#define TAM_BUFER (1024*TAM_EVENTO)

t_list *lista_cajas;
t_log *logger; //R-W
t_list *list_personajes; //R-W
t_list *list_items; //R-W
char* nom_nivel; //R
int cantRecursos; //R
int maxRows=0, maxCols=0; //Del area del nivel //R
message_t msj;

int cant_enemigos; //R
int sleep_enemigos; //R
int hayQueAsesinar = true;
char* dir_plataforma;
int recovery;
char* algoritmo;
int quantum;
int retardo;
int timeCheck;
char* ip_plataforma;
char* port_orq;

pthread_mutex_t semNivel; //R -W
pthread_mutex_t mutexEnemigos;
pthread_mutex_t semaforo1;

void levantarArchivoConf(char*);

int main(int argc, char *argv[]) {
	signal(SIGINT, cerrarForzado);
	char buferNotify[TAM_BUFER];
	int i,vigilante,rv,descriptorNotify,sockOrq,sockPlanif;
	struct pollfd uDescriptores[2];
	pers_t pjNew;
	pthread_mutex_init(&semNivel, NULL );
	pthread_mutex_init(&mutexEnemigos, NULL );
	pthread_mutex_init(&semaforo1,NULL);
	int posX = 0, posY = 0;// Para los personajes
	int posRecY = 0, posRecX = 0;// Para los recursos

	//LOG
	logger = logInit(argv, "NIVEL");

	// INICIALIZANDO GRAFICA DE MAPAS
	list_items = list_create();
	list_personajes = list_create();
	nivel_gui_inicializar();
	nivel_gui_get_area_nivel(&maxRows, &maxCols);

	//LEVANTAR EL ARCHIVO CONFIGURACION EN VARIABLES GLOBALES
	levantarArchivoConf(argv[1]);

	//--------------------SALUDO - INFO - INFO_PLANIFICADOR - WHATS_UP--------------------//
	// Definiciones para el uso de sockets
	sockOrq = connectServer(ip_plataforma, atoi(port_orq), logger, "orquestador");
	// Armo mensaje inicial de SALUDO con el orquestador
	orq_t orqMsj;
	orqMsj.type = NIVEL;
	orqMsj.detail = SALUDO;
	strcpy(orqMsj.name, nom_nivel);
	// Envía el "Saludo" al orquestador
	enviaMensaje(sockOrq, &orqMsj, sizeof(orq_t), logger, "Saludo Orquestador");
	// Arma mensaje INFO con la informacion para pasarle al planificador
	orqMsj.type = INFO;
	orqMsj.detail = quantum;
	orqMsj.port = retardo;
	strcpy(orqMsj.name, algoritmo);
	// Envía msj INFO y el orquestador lanza el hilo planificador para este nivel
	enviaMensaje(sockOrq, &orqMsj, sizeof(orq_t), logger, "Info de Planificacion");

	//-------------Recibe puerto e ip del planificador para hacer el connect------------//
	//Esto se podría sacar y directamente hacer un listen y accept, pero es mejor con solo un connect
	recibeMensaje(sockOrq, &orqMsj, sizeof(orq_t), logger, "Recibi puerto de mi planificador");
	int puertoPlan;
	if (orqMsj.type == INFO_PLANIFICADOR) {
		puertoPlan = orqMsj.port;
		if (!string_equals_ignore_case(orqMsj.ip, ip_plataforma)) {
			log_warning(logger, "WARN: Las ip del archivo config y la que recibo del orquestador no coinciden");
			exit(EXIT_FAILURE);
		}

	} else {
		log_error(logger, "Tipo de mensaje incorrecto: se esperaba INFO_PLANIFICADOR del orquestador");
		exit(EXIT_FAILURE);
	}

	//-------------Recibe puerto e ip del planificador para hacer el connect------------//
	//Me conecto al planificador
	sockPlanif = connectServer(ip_plataforma, puertoPlan, logger, "planificador");
	//Fuerzo un envio de mensaje al planificador para que me agregue a su lista de sockets y pueda mandar mensajes

	msj.type = NIVEL;
	msj.detail = WHATS_UP;
	enviaMensaje(sockPlanif, &msj, sizeof(message_t), logger, "Whats up man");
	// Logueo la conexion con el orquestador
	log_info(logger, "Conexión con el planificador con puerto %d", puertoPlan);

		//INOTIFY
		descriptorNotify=inotify_init();
		vigilante=inotify_add_watch(descriptorNotify,argv[1],IN_MODIFY);
		if(vigilante==1) puts("error en inotify add_watch");

		//POLL
		uDescriptores[0].fd=sockPlanif;
		uDescriptores[0].events=POLLIN;
		uDescriptores[1].fd=descriptorNotify;
		uDescriptores[1].events=POLLIN;

	////CREANDO Y LANZANDO HILOS ENEMIGOS
	threadEnemy_t *hilosEnemigos;
	hilosEnemigos = calloc(cant_enemigos, sizeof(threadEnemy_t));
	for (i = 0; i < cant_enemigos; i++) {
		hilosEnemigos[i].enemy.num_enemy = i + 1; //El numero o id de enemigo
		hilosEnemigos[i].enemy.sockP = sockPlanif;
		if (pthread_create(&hilosEnemigos[i].thread_enemy, NULL, enemigo,(void*) &hilosEnemigos[i].enemy)) {
			log_error(logger, "pthread_create: %s", strerror(errno));
			exit(EXIT_FAILURE);
		}
	}

	while (1) {
		//esperarMensaje(sockPlanif, &msj, sizeof(msj), logger);
		int contPj;
		pers_t * personajeAux;
		pthread_mutex_lock(&semNivel);//---------->???????????????????

		if((rv=poll(uDescriptores,2,-1))==-1) perror("poll");
		else{
			if (uDescriptores[1].revents&POLLIN){
				read(descriptorNotify,buferNotify,TAM_BUFER);
				struct inotify_event* evento=(struct inotify_event*) &buferNotify[0];
				if(evento->mask & IN_MODIFY){
					levantarArchivoConf(argv[1]);

					//avisar a planificador que cambio el archivo config
					exit(EXIT_FAILURE);
				}
			}
			if(uDescriptores[0].revents & POLLIN){
				recibeMensaje(sockPlanif,&msj,sizeof(msj),logger,"recibiendo mensajes de plataforma");
				switch (msj.type) {
				case SALUDO: //El planificador SALUDA al nivel pasandole el nuevo personaje que quiere jugar
					//Creo el personaje en el mapa
					CrearPersonaje(list_items, msj.name, INI_X, INI_Y);
					// TODO validar que no haya otro personaje con el mismo simbolo jugando en el nivel
					pjNew.simbolo  = msj.name;
					pjNew.blocked  = true;
					//pjNew.muerto   = false;
					pjNew.recursos = list_create();
					// Logueo el personaje recien agregado
					log_info(logger, "Se agregó al personaje %c", pjNew.simbolo);
					// Devuelvo msj SALUDO al planificador.
					msj.type    = NIVEL;
					msj.detail  = INI_X;
					msj.detail2 = INI_Y;
					// Envio la posicion inicial del personaje al planificador
					enviaMensaje(sockPlanif, &msj, sizeof(message_t), logger,"Posicion inicial");
					// Agrego el personaje a la lista de personajes del nivel
					list_add_new(list_personajes, (void *) &pjNew, sizeof(pers_t));
				break;
				case POSICION_RECURSO:
					// El personaje le pidio la posicion del siguiente recurso a buscar al planificador, y este me lo pide a mi
					// Busco la posicion del recurso pedido en el mapa
					getPosRecurso(list_items, msj.detail2, &posRecX, &posRecY);
					msj.type = POSICION_RECURSO;
					msj.detail = posRecX;
					msj.detail2 = posRecY;
					// Envio mensaje al planificador con la posicion para que éste le mande la posicion al personaje
					enviaMensaje(sockPlanif, &msj, sizeof(message_t), logger,"Posicion del recurso");
				break;
				case MOVIMIENTO:
					// Busco la posicion actual del personaje
					getPosPersonaje(list_items, msj.name, &posX, &posY);
					//Buscar la posicion del recurso que esta presiguiendo el personaje
					getPosRecurso(list_items, msj.detail, &posRecX, &posRecY);
					// calculo el movimiento
					switch (msj.detail2) {
						case ARRIBA:
							if (posY > 1) posY--;
							break;
						case ABAJO:
							if (posY < maxRows) posY++;
							break;
						case IZQUIERDA:
							if (posX > 1) posX--;
							break;
						case DERECHA:
							if (posX < maxCols) posX++;
							break;
					}

					MoverPersonaje(list_items, msj.name, posX, posY);

					//TODO desbloquear, para el deadlock es esto
					//Si llegó al recurso
					if ((posX == posRecX) && (posY == posRecY)) {
						//Agrego el recurso a la lista de recursos del personaje
						for (contPj = 0; contPj < list_size(list_personajes); contPj++) {
							// Recorro la lista y voy levantado personajes
							personajeAux = (pers_t *) list_get(list_personajes, contPj);
							if (personajeAux->simbolo == msj.name) {
								// Agrego el recurso pedido a su lista de recursos
								list_add_new(personajeAux->recursos, &(msj.detail),	sizeof(char));
								break;
							}
						}
						// Calculo la cantidad de instancias
						int cantInstancias = restarInstanciasRecurso(list_items,msj.detail);
						if (cantInstancias >= 0) {
							// Loqueo que al personaje se le dio un recurso
							log_info(logger, "Al personaje %c se le dio el recurso %c",personajeAux->simbolo, msj.detail);
							personajeAux->blocked = false;
							//msj.type = MOVIMIENTO; //El msj.type se setea cuando verifica pos de enemigos
							msj.detail2 = msj.detail;
							msj.detail = OTORGADO;
							msj.name = personajeAux->simbolo;
							// Envio mensaje donde confirmo la otorgacion del recurso pedido
							enviaMensaje(sockPlanif, &msj, sizeof(message_t), logger,"Se otorgo el recurso pedido");
						} else {
							// Logueo el bloqueo del personaje
							log_info(logger,"El personaje %c se bloqueo por el recurso %c",personajeAux->simbolo, msj.detail);
							//Lo pongo como bloqueado
							personajeAux->blocked = true;
							//msj.type = MOVIMIENTO; //El msj.type se setea cuando verifica pos de enemigos
							msj.detail2 = msj.detail;
							msj.detail = BLOCK;
							msj.name = personajeAux->simbolo;
							// Envio mensaje donde denego el pedido del recurso
							enviaMensaje(sockPlanif, &msj, sizeof(message_t), logger,"Se denego el pedido del recurso");
						}
					}else { //Si no llego al recurso sigue moviendose tranquilamente
						//msj.type = MOVIMIENTO; //El msj.type se setea cuando verifica pos de enemigos
						msj.detail2 = msj.detail;
						msj.detail = NADA;
						msj.name = personajeAux->simbolo;
						// Envio mensaje donde denego el pedido del recurso
						enviaMensaje(sockPlanif, &msj, sizeof(message_t), logger,"Se movio el personaje;");
					}
				break;
				case SALIR:
					// Un personaje termino o murio y debo liberar instancias de recursos que tenia asignado
					for (contPj = 0; contPj < list_size(list_personajes); contPj++) {
						personajeAux = (pers_t *) list_get(list_personajes, contPj);
						// Recorro los personajes
						if (personajeAux->simbolo == msj.name) {
							liberarRecursos(personajeAux, contPj);
							//Armo mensaje SALIR y confirmo al planificador la salida del personaje
							msj.type = SALIR;
							enviaMensaje(sockPlanif, &msj, sizeof(msj), logger,"Confirma salir al planificador");
							// Salgo del ciclo de recorrer personajes
				          break;
			             }
					}
				break;
				} //Fin del switch
			}
		//nivel_gui_dibujar(list_items, nom_nivel);------------>DIBUJAN LOS HILOS ENEMIGOS
		}
		pthread_mutex_unlock(&semNivel);

	}
	inotify_rm_watch(descriptorNotify,vigilante);
	close(descriptorNotify);
	return 0;
}
void levantarArchivoConf(char* argumento){
	t_config *configNivel; //TODO destruir el config cuando cierra el nivel,
	char **arrCaja;
	char* cajaAux;
	cajaAux=malloc(sizeof(char) * 9);
	int t=1,posXCaja,posYCaja;
	char* dir_plataforma;
	char* messageLimitErr= malloc(sizeof(char) * 100);

	sprintf(cajaAux, "Caja1");
	configNivel = config_try_create(argumento, "Nombre,puerto,ip,Plataforma,TiempoChequeoDeadlock,Recovery,Enemigos,Sleep_Enemigos,algoritmo,quantum,retardo,Caja1");
	// Mientras pueda levantar el array
	while ((arrCaja = config_try_get_array_value(configNivel, cajaAux)) != NULL ) {
		// Convierto en int las posiciones de la caja
		posXCaja = atoi(arrCaja[3]);
		posYCaja = atoi(arrCaja[4]);
		// Validamos que la caja a crear esté dentro de los valores posibles del mapa
		if (posYCaja > maxRows || posXCaja > maxCols || posYCaja < 1 || posXCaja < 1) {
			sprintf(messageLimitErr, "La caja %c excede los limites de la pantalla. (%d,%d) - (%d,%d)", arrCaja[1][0], posXCaja, posYCaja, maxRows, maxCols);
			cerrarNivel(messageLimitErr);
			exit(EXIT_FAILURE);
		}
		// Si la validacion fue exitosa creamos la caja de recursos
		CrearCaja(list_items, arrCaja[1][0], atoi(arrCaja[3]), atoi(arrCaja[4]), atoi(arrCaja[2]));
		// Rearma el cajaAux para la iteracion
		sprintf(cajaAux, "Caja%d", ++t);
		// Armo estructura de lista
	}
	free(messageLimitErr);
	free(cajaAux);

	nom_nivel 	   = config_get_string_value(configNivel, "Nombre");
	dir_plataforma = config_get_string_value(configNivel, "Plataforma");
	recovery       = config_get_int_value(configNivel, "Recovery");
	cant_enemigos  = config_get_int_value(configNivel, "Enemigos");
	sleep_enemigos = config_get_int_value(configNivel, "Sleep_Enemigos");
	algoritmo      = config_get_string_value(configNivel, "algoritmo");
	quantum 	   = config_get_int_value(configNivel, "quantum");
	retardo 	   = config_get_int_value(configNivel, "retardo");
	timeCheck      = config_get_int_value(configNivel, "TiempoChequeoDeadlock");
	ip_plataforma  = strtok(dir_plataforma, ":");
	port_orq 	   = strtok(NULL, ":");
	cantRecursos   = list_size(list_items);
}
void *enemigo(void * args) {
	enemigo_t *enemigo;
	enemigo = (enemigo_t *) args;

	int cantPersonajesActivos;
	int contMovimiento = 1;
	char victimaAsignada='0';
	char ultimoMov='a';
	char dirMov='b';
	int dist1,dist2=9999999,i;

	ITEM_NIVEL* item;
	//chequeando que la posicion del enemigo no caiga en un recurso
	enemigo->posX = 1+(rand() % maxCols);
	enemigo->posY = 1+(rand() % maxRows);
	bool esUnRecurso(ITEM_NIVEL *item){ return (item->item_type==RECURSO_ITEM_TYPE&&item->posx==enemigo->posX&&item->posy==enemigo->posY);}
	while(list_any_satisfy(list_items,(void*)esUnRecurso)){
		enemigo->posX=1+(rand() % maxCols);
		enemigo->posY=1+(rand() % maxRows);
	}
	CreateEnemy(list_items,enemigo->num_enemy,enemigo->posX,enemigo->posY);

	while (1) {
		bool personajeBloqueado(pers_t* personaje){return(personaje->esperandoRec==false);}
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
			void esUnRecurso2(ITEM_NIVEL *item){if (item->item_type==RECURSO_ITEM_TYPE&&item->posx==enemigo->posX&&item->posy==enemigo->posY) enemigo->posX--;}
			list_iterate(list_items,(void*)esUnRecurso2);
		} else { //ELEGIR O PERSEGUIR A LA VICTIMA
			pers_t* persVictima;
			if(victimaAsignada=='0'){     //reordenar viendo cual es la victima mas cercana
				for(i=0;i<cantPersonajesActivos;i++){
					persVictima=list_get(list_personajes,i);
					if(persVictima->esperandoRec==false){ //el personaje no esta quieto esperando por un recurso
						bool esElPersonaje(ITEM_NIVEL* personaje){return(personaje->id==persVictima->simbolo);}
						item=list_find(list_items,(void*)esElPersonaje);
						dist1=(enemigo->posX-item->posx)*(enemigo->posX-item->posx)+(enemigo->posY-item->posy)*(enemigo->posY-item->posy);
						if(dist1<dist2){
							victimaAsignada=item->id;
							dist2=dist1;
						}
					}
				}
			}else{//ya teiene una victima asignada
				bool unPersonaje(ITEM_NIVEL* item){	return (item->id==victimaAsignada);	}
				item=list_find(list_personajes,(void*)unPersonaje);
				if(enemigo->posY==item->posy){
					if(enemigo->posX<item->posx) contMovimiento=1;
					if(enemigo->posX>item->posx){ contMovimiento=3;}
					else {//se esta en la misma posicion que la victima =>matarla
						log_debug(logger, "El personaje %c esta muerto", msj.name);
						msj.type = MOVIMIENTO;
						msj.detail = MUERTO_ENEMIGOS;
						//msj.name= yaesta el simbolo
						enviaMensaje(enemigo->sockP, &msj, sizeof(msj), logger,	"Se mato a alguien :P");


						//KillPersonaje(list_personajes,item->id);
						victimaAsignada='0';
						}
					}else{ //acercarse por fila
						if(enemigo->posY<item->posy) contMovimiento=4;
						if(enemigo->posY>item->posy) contMovimiento=2;
						}
				//ver si el personaje que estaba persiguiendo se bloqueo en un recurso
				bool buscarPersonaje(pers_t personaje){return (personaje.simbolo==item->id);}
				persVictima=list_find(list_personajes,(void*)buscarPersonaje);
				if(persVictima->esperandoRec==true){//elegir otra victima
					victimaAsignada='0';
				}
				actualizaPosicion(&contMovimiento, &(enemigo->posX),&(enemigo->posY));
				void esUnRecurso2(ITEM_NIVEL *item){
					if (item->item_type==RECURSO_ITEM_TYPE&&item->posx==enemigo->posX&&item->posy==enemigo->posY)
						enemigo->posX--;
					}
				list_iterate(list_items,(void*)esUnRecurso2);
			}
		} //Fin de else
		pthread_mutex_lock(&semaforo1);
		MoveEnemy(list_items, enemigo->num_enemy, enemigo->posX,enemigo->posY);
		nivel_gui_dibujar(list_items, nom_nivel);
		pthread_mutex_unlock(&semaforo1);
		usleep(sleep_enemigos);
	} //Fin de while(1)
	pthread_exit(NULL );
}
void *deteccionInterbloqueo (void *parametro){
	t_caja* caja;
	int i,j,k,fila;
	ITEM_NIVEL* item;
	pers_t* personaje;
	struct timespec dormir;
	dormir.tv_sec=(time_t)(timeCheck/1000);
	dormir.tv_nsec=(long)((timeCheck%1000)*1000000);

	while(1){
		pthread_mutex_lock (&semaforo1);//nadie se mueve hasta que no se evalue el interbloqueo
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
		for(i=0;i<cantPersonajes;i++){
			personaje=list_get(list_personajes,i);
			for(j=0;j<list_size(personaje->recursos);j++){
				caja=list_get(personaje->recursos,j);
				for(k=0;k<cantRecursos;k++){
					if(vecCajas[k].simbolo==caja->simbolo){
						if((personaje->blocked==true)&&(j==list_size(personaje->recursos)-1)){
							matSolicitud[i][k]+=1;
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
			//NO HAY INTERBLOQUEO
		}else{
			if(recovery==1){
				//notificar a plataforma
			}
		}
		pthread_mutex_unlock (&semaforo1);
		nanosleep(&dormir,NULL);
	}
	return 0;
}
/*
void KillPersonaje(t_list *list_personajes, char name) {
	int i;
	pers_t *pjAux;
	for (i = 0; i < list_size(list_personajes); i++) {
		pjAux = list_get(list_personajes, i);
		if (pjAux->simbolo == name)
			pjAux->muerto = true;
	}
}
*/
/*
_Bool personajeMuerto(t_list *list_personajes, char name) {
	int i;
	pers_t *pjAux;
	for (i = 0; i < list_size(list_personajes); i++) {
		pjAux = (pers_t *) list_get_data(list_personajes, i);
		if (pjAux->simbolo == name && pjAux->muerto)
			return true;
	}
	return false;
}
*/
/*
void matar(enemigo_t *enemigo, pers_t *pjVictima, int indice, char*ip_plataforma, int puertoPlan) {
	message_t *msj = malloc(sizeof(message_t));
	msj->type = NIVEL;
	msj->detail = MUERTO_ENEMIGOS;
	msj->name = pjVictima->simbolo;

	liberarRecursos(pjVictima, indice);
	hayQueAsesinar = false;

	enemigo->sockP = connectServer(ip_plataforma, puertoPlan, logger, "planificador");

	enviaMensaje(enemigo->sockP, &msj, sizeof(msj), logger, "Informo asesinato al planificador");
	recibeMensaje(enemigo->sockP, &msj, sizeof(msj), logger, "Recibo confirmacion");
	free(msj);
}*/
/*
bool hayAlgunEnemigoArriba(int posPerX, int posPerY) {

	int i;
	int posEnemyX, posEnemyY;
	for (i = 1; i <= cant_enemigos; i++) {
		getPosEnemy(list_items, i, &posEnemyX, &posEnemyY);
		if (posPerX == posEnemyX && posPerY == posEnemyY)
			return true;
	}
	return false;
}*/
/*
pers_t* hayAlgunEnemigoArribaDeAlgunPersonaje() {
	int i, j;
	int posEnemyX, posEnemyY;
	int posX, posY;
	for (i = 1; i <= cant_enemigos; i++) {

		getPosEnemy(list_items, i, &posEnemyX, &posEnemyY);
		for (j = 0; j < list_size(list_personajes); j++) {

			pers_t *pjLev = (pers_t *) list_get(list_personajes, j);
			getPosPersonaje(list_items, pjLev->simbolo, &posX, &posY);
			if (posX == posEnemyX && posY == posEnemyY)
				return pjLev;
		}
	}
	return NULL ;
}
*/
void liberarRecursos(pers_t *personajeAux, int index_l_personajes) {

	int contRec;
	char* auxRec;
	// Si lo encontre recorro su lista de recursos y voy liberando lo que tenia
	for (contRec = 0; contRec < list_size(personajeAux->recursos); contRec++) {
		auxRec = (char *) list_get(personajeAux->recursos, contRec);
		sumarInstanciasRecurso(list_items, *auxRec);
	}

	//Destruyo la lista y los elementos de la lista: libera memoria
	list_destroy_and_destroy_elements(personajeAux->recursos, (void *) recurso_destroyer);

	/*if (personajeAux->muerto) { //Esto es porque si lo mato un enemigo que no intente borrarlo dos veces
		hayQueAsesinar = true;
		log_debug(logger, "El personaje %c esta muerto y reinicia", personajeAux->simbolo);
	}*/

	BorrarItem(list_items, personajeAux->simbolo);

	// Loqueo la desconexion del personaje
	log_debug(logger, "El personaje %c se desconecto", personajeAux->simbolo);

	list_remove(list_personajes, index_l_personajes);

	personaje_destroyer(personajeAux);

}

void moverme(int *victimaX, int *victimaY, int *posX, int *posY,
		mov_t *movimiento) {

	if (*victimaX > *posX) {
		(*posX)++;
		movimiento->type_mov_x = derecha;
		movimiento->in_x = true;
	} else {
		if (*victimaX < *posX) {
			(*posX)--;
			movimiento->type_mov_x = izquierda;
			movimiento->in_x = true;
		} else
			movimiento->in_x = false;
	}

	if (*victimaY > *posY) {
		(*posY)++;
		movimiento->type_mov_y = abajo;
		movimiento->in_y = true;
	} else {
		if (*victimaY < *posY) {
			(*posY)--;
			movimiento->type_mov_y = arriba;
			movimiento->in_y = true;
		} else
			movimiento->in_y = false;
	}

}
/*
void validarPosSobreRecurso(t_list *list_items, mov_t movimiento, int *posX, int *posY) {

	int i;
	ITEM_NIVEL *aux;

	for (i = 0; i < list_size(list_items); i++) {

		aux = (ITEM_NIVEL *) list_get(list_items, i);

		if (aux->item_type == RECURSO_ITEM_TYPE) {

			//Si estoy arriba de una caja de recursos, entonces la esquivo
			if (*posX == aux->posx && *posY == aux->posy) {

				//Si me movi en el eje X
				if (movimiento.in_x && !movimiento.in_y) {

					if (movimiento.type_mov_x == izquierda) {
						(*posX)++; //Vuelvo atras
						(*posY)--; //Subo un escalon
					} else {
						(*posX)--; //Vuelvo un paso atras
						(*posY)++; //Lo esquivo por abajo
					}
				}
				//Si me movi en el eje Y
				if (movimiento.in_y && !movimiento.in_x) {

					if (movimiento.type_mov_y == arriba) {
						(*posY)++; //Vuelvo atras
						(*posX)++; //Me muevo a la derecha
					} else {
						(*posY)--; //Vuelvo un paso atras
						(*posX)--; //Lo esquivo por la izquierda

					}
				}

				if (movimiento.in_x && movimiento.in_y) { //diagonales
					if (movimiento.type_mov_x == izquierda) { //diagonal derecha superior
						if (movimiento.type_mov_y == abajo)
							(*posY)++; //Subo un escalon: arriba del recurso estoy ahora
						else if (movimiento.type_mov_y == arriba) //diagonal derecha inferior
							(*posY)--;
					}

					if (movimiento.type_mov_x == derecha) {
						if (movimiento.type_mov_y == abajo)
							(*posY)--;
						else if (movimiento.type_mov_y == arriba)
							(*posY)--;
					}

				}

			} //Fin del if de validacion de mi pos contra la de una caja de recursos

		} //Fin del if de validacion de si es un recurso

	} //Fin del for de recorrido de recursos
}
*/
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

static void recurso_destroyer(char *recurso) {
	free(recurso);
}
