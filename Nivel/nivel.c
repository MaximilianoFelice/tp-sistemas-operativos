//export LD_LIBRARY_PATH=/home/utnso/git/fuerzas_ginyu/libs/commons/Debug:/home/utnso/git/fuerzas_ginyu/libs/ginyu/Debug:/home/utnso/git/fuerzas_ginyu/libs/commons/Debug:/home/utnso/git/fuerzas_ginyu/libs/gui/Debug

// PARA EJECUTAR NIVEL: NO EJECUTAR CON -v NI CON -ll trace PORQUE SINO DIBUJA MAL
// ./nivel ../nivel1.config

#include "nivel.h"

t_log *logger; //R-W
t_list *list_personajes; //R-W
t_list *list_items; //R-W
char* nom_nivel; //R
int cantRecursos; //R
int maxRows, maxCols; //Del area del nivel //R

//Para los enemigos
int cant_enemigos; //R
int sleep_enemigos; //R
int hayQueAsesinar = true;

pthread_mutex_t semNivel; //R -W
pthread_mutex_t mutexEnemigos;

int main(int argc, char *argv[]) {
	// Creacion de variables
	t_config *configNivel; //TODO destruir el config cuando cierra el nivel,

	signal(SIGINT, cerrarForzado);

	char **arrCaja;
	int t = 1;  // Variable para el ciclo de recursos
	int cols = 0;
	int rows = 0;
	char* dir_plataforma;
	int posXCaja = 0;
	int posYCaja = 0;

	pthread_mutex_init(&semNivel, NULL );
	pthread_mutex_init(&mutexEnemigos, NULL );

	// Inicializa la lista de items que usa para dibujar
	list_items = list_create();

	// Inicializa la lista de personajes con sus recursos
	list_personajes = list_create();
	pers_t pjNew;

	// Inicializa el logger
	logger = logInit(argv, "NIVEL");

	// Creamos el config
	configNivel =
			config_try_create(argv[1],
					"Nombre,puerto,ip,Plataforma,TiempoChequeoDeadlock,Recovery,Enemigos,Sleep_Enemigos,algoritmo,quantum,retardo,Caja1");

	nivel_gui_inicializar();

	// Conseguimos el area del nivel
	char state = nivel_gui_get_area_nivel(&rows, &cols); //TODO cambie de lugar los argumentos

	// Validamos que no haya habido error
	if (state != EXIT_SUCCESS)
		cerrarNivel("Error al conseguir el área del nivel");

	// Variable para mensajes de error
	char* messageLimitErr;
	messageLimitErr = malloc(sizeof(char) * 100);

	// Creamos cada caja de recursos
	char* cajaAux;
	cajaAux = malloc(sizeof(char) * 9);
	sprintf(cajaAux, "Caja1");

	// Mientras pueda levantar el array
	while ((arrCaja = config_try_get_array_value(configNivel, cajaAux))
			!= NULL ) {
		// Convierto en int las posiciones de la caja
		posXCaja = atoi(arrCaja[3]);
		posYCaja = atoi(arrCaja[4]);

		// Validamos que la caja a crear esté dentro de los valores posibles del mapa
		if (posYCaja > rows || posXCaja > cols || posYCaja < 1
				|| posXCaja < 1) { //TODO cambie de lugar las X e Y en posYCaja y posXCaja
			sprintf(messageLimitErr,
					"La caja %c excede los limites de la pantalla. (%d,%d) - (%d,%d)",
					arrCaja[1][0], posXCaja, posYCaja, rows, cols);
			cerrarNivel(messageLimitErr);
			exit(EXIT_FAILURE);
		}

		// Si la validacion fue exitosa creamos la caja de recursos
		CrearCaja(list_items, arrCaja[1][0], atoi(arrCaja[3]), atoi(arrCaja[4]),
				atoi(arrCaja[2]));

		// Rearma el cajaAux para la iteracion
		sprintf(cajaAux, "Caja%d", ++t);

		// Armo estructura de lista
	}

	// Liberamos memoria
	free(messageLimitErr);
	free(cajaAux);

	cantRecursos = list_size(list_items);

	// Obtenemos el string del nombre y dirección de la plataforma
	nom_nivel = config_get_string_value(configNivel, "Nombre");
	dir_plataforma = config_get_string_value(configNivel, "Plataforma");

	// Obtenemos el valor de recovery
	int recovery = config_get_int_value(configNivel, "Recovery");

	// Obtenemos datos de enemigos
	cant_enemigos = config_get_int_value(configNivel, "Enemigos");

	sleep_enemigos = config_get_int_value(configNivel, "Sleep_Enemigos");

	// Variables auxiliares para levantar posiciones de recursos
	int i;
	int posEnemigoX, posEnemigoY;

	//Obtengo el area del mapa del nivel
	srand(time(NULL ));

	// Consigo el area de la ventana del nivel
	nivel_gui_get_area_nivel(&maxRows, &maxCols);

	// Inicializo a los enemigos en el mapa
	for (i = 0; i < cant_enemigos; i++) {

		//numero = (rand() % limite_superior) + limite_inferior;
		posEnemigoX = (rand() % (cols));
		posEnemigoY = (rand() % (rows)) + 1;

		//PRIMERO: me aseguro que no comience en el origen
		if (posEnemigoX <= 4 && posEnemigoY <= 4) {
			while (posEnemigoX <= 4 && posEnemigoY <= 4) {
				posEnemigoX = (rand() % (cols));
				posEnemigoY = (rand() % (rows)) + 1;
			}
		}

		// Valido la posicion en los limites del eje X
		if (posEnemigoX >= maxCols) {
			while (1) {
				posEnemigoX = (rand() % (cols));
				if (posEnemigoX < maxCols)
					break;
			}
		}

		// Valido la posicion en los limites del eje X
		if (posEnemigoY >= maxRows) {
			while (1) {
				posEnemigoY = (rand() % (rows)) + 1;
				if (posEnemigoY < maxRows)
					break;
			}
		}

		//SEGUNDO: Me fijo que no estén sobre un recurso
		ITEM_NIVEL *itemAux; //Aqui levanto la data

		// Defino un puntero auxiliar a mi lista de items
		t_list * temp;
		temp = list_items;

		t_link_element *element = temp->head;

		// Comparo mi posicion contra la de cada
		// otro item(en este caso puede ser un recurso o un enemigo antes agregado)
		while (element != NULL ) {
			// Levanto el item de la lista
			itemAux = (ITEM_NIVEL *) element->data;

			// Me fijo si esta en la misma posicion del recurso
			if ((itemAux->posx == posEnemigoX)
					&& (itemAux->posy == posEnemigoY)) {
				//Me aseguro que no esten en un recurso y dentro de los limites del mapa
				while (((itemAux->posx == posEnemigoX)
						&& (itemAux->posy == posEnemigoY))
						|| (posEnemigoX >= maxCols || posEnemigoY >= maxRows)) {
					posEnemigoX = (rand() % (cols));
					posEnemigoY = (rand() % (rows)) + 1;
				}
				break; //Sale de este while de recorrido de recursos
			}
			element = element->next; //Paso al siguiente elemento
			itemAux = NULL;
		}

		//Para mover y crear enemigos hay que usar las funciones MoveEnemy() y CreateEnemy() respectivamente
		CreateEnemy(list_items, i + 1, posEnemigoX, posEnemigoY);

	} //Termine de poner los recursos y enemigos en la lista para graficar

	// Obtenemos el resto de los datos del archivo config
	char * algoritmo = config_get_string_value(configNivel, "algoritmo");
	int quantum = config_get_int_value(configNivel, "quantum");
	int retardo = config_get_int_value(configNivel, "retardo");
	int timeCheck = config_get_int_value(configNivel, "TiempoChequeoDeadlock");
	char *ip_plataforma = strtok(dir_plataforma, ":");  //Separo la ip
	char* port_orq = strtok(NULL, ":");			 //Separo el puerto

	//--------------------SALUDO - INFO - INFO_PLANIFICADOR - WHATS_UP--------------------//

	// Definiciones para el uso de sockets
	int sockOrq = connectServer(ip_plataforma, atoi(port_orq), logger,
			"orquestador");

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
	enviaMensaje(sockOrq, &orqMsj, sizeof(orq_t), logger,
			"Info de Planificacion");

	//-------------Recibe puerto e ip del planificador para hacer el connect------------//
	//Esto se podría sacar y directamente hacer un listen y accept, pero es mejor con solo un connect
	recibeMensaje(sockOrq, &orqMsj, sizeof(orq_t), logger,
			"Recibi puerto de mi planificador");

	int puertoPlan;

	if (orqMsj.type == INFO_PLANIFICADOR) {
		puertoPlan = orqMsj.port;
		if (!string_equals_ignore_case(orqMsj.ip, ip_plataforma)) {
			log_warning(logger,
					"WARN: Las ip del archivo config y la que recibo del orquestador no coinciden");
			exit(EXIT_FAILURE);
		}

	} else {
		log_error(logger,
				"Tipo de mensaje incorrecto: se esperaba INFO_PLANIFICADOR del orquestador");
		exit(EXIT_FAILURE);
	}
	//-------------Recibe puerto e ip del planificador para hacer el connect------------//

	//Me conecto al planificador
	int sockPlanif = connectServer(ip_plataforma, puertoPlan, logger,
			"planificador");

	//Fuerzo un envio de mensaje al planificador para que me agregue a su lista de sockets y pueda mandar mensajes

	message_t msj;
	msj.type = NIVEL;
	msj.detail = WHATS_UP;

	enviaMensaje(sockPlanif, &msj, sizeof(message_t), logger, "Whats up man");

	// Logueo la conexion con el orquestador
	log_info(logger, "Conexión con el planificador con puerto %d", puertoPlan);

	//--------------------SALUDO - INFO - INFO_PLANIFICADOR - WHATS_UP--------------------//

	// Hice una estructura distinta para los hilos de enemigos
	// porque sinó habia problemas con los parámetros que se le pasaba al hilo.

	//Dibujo la lista de items
	nivel_gui_dibujar(list_items, nom_nivel);

	threadEnemy_t *hilosEnemigos;
	hilosEnemigos = calloc(cant_enemigos, sizeof(threadEnemy_t));

	//Armo estructura y tiro los hilos enemigos
	for (i = 0; i < cant_enemigos; i++) {

		hilosEnemigos[i].enemy.num_enemy = i + 1; //El numero o id de enemigo
		hilosEnemigos[i].enemy.sockP = sockPlanif;

		if (pthread_create(&hilosEnemigos[i].thread_enemy, NULL, enemigo,(void*) &hilosEnemigos[i].enemy)) {
			log_error(logger, "pthread_create: %s", strerror(errno));
			exit(EXIT_FAILURE);
		}
	}

	// Para los personajes
	int posX = 0, posY = 0;
	// Para los recursos
	int posRecY = 0, posRecX = 0;

	//TODO tirar hilo de interbloqueo

	//TODO tirar hilo de iNotify

	while (1) {

		esperarMensaje(sockPlanif, &msj, sizeof(msj), logger);

		// Definicion de variables
		int contPj;
		pers_t * personajeAux;

		pthread_mutex_lock(&semNivel);

		//El planificador le envia el tipo del mensaje en msj.type
		switch (msj.type) {
		case SALUDO: //El planificador SALUDA al nivel pasandole el nuevo personaje que quiere jugar

			//Creo el personaje en el mapa
			CrearPersonaje(list_items, msj.name, INI_X, INI_Y);
			// TODO validar que no haya otro personaje con el mismo simbolo jugando en el nivel
			pjNew.simbolo = msj.name;
			pjNew.blocked = false;
			pjNew.muerto = false;
			pjNew.recursos = list_create();

			// Logueo el personaje recien agregado
			log_info(logger, "Se agregó al personaje %c", pjNew.simbolo);

			// Devuelvo msj SALUDO al planificador.
			msj.type = NIVEL;
			msj.detail = INI_X;
			msj.detail2 = INI_Y;

			// Envio la posicion inicial del personaje al planificador
			enviaMensaje(sockPlanif, &msj, sizeof(message_t), logger,"Posicion inicial");

			// Agrego el personaje a la lista de personajes del nivel
			list_add_new(list_personajes, (void *) &pjNew, sizeof(pers_t));
			break;

		case POSICION_RECURSO:
			// El personaje le pidio la posicion del siguiente recurso a
			// buscar al planificador, y este me lo pide a mi
			// Busco la posicion del recurso pedido en el mapa

			getPosRecurso(list_items, msj.detail2, &posRecX, &posRecY);

			// Armo mensaje para darle la posicion del recurso
			msj.type = POSICION_RECURSO;
			msj.detail = posRecX;
			msj.detail2 = posRecY;

			// Envio mensaje al planificador con la posicion para
			// que éste le mande la posicion al personaje
			enviaMensaje(sockPlanif, &msj, sizeof(message_t), logger,"Posicion del recurso");

			break;

		case MOVIMIENTO:

			//Validacion1
			if (personajeMuerto(list_personajes, msj.name)) { //Fue matado por un enemigo
				log_debug(logger, "El personaje %c esta muerto", msj.name);
				msj.type = MOVIMIENTO;
				msj.detail = MUERTO_ENEMIGOS;
				//msj.name= yaesta el simbolo

				enviaMensaje(sockPlanif, &msj, sizeof(msj), logger,	"Se mato a alguien :P");
				break;
			}

			// Busco la posicion actual del personaje
			getPosPersonaje(list_items, msj.name, &posX, &posY);

			//Validacion2
			if (hayAlgunEnemigoArriba(posX, posY)) {
				log_debug(logger, "El personaje %c esta muerto", msj.name);
				msj.type = MOVIMIENTO;
				msj.detail = MUERTO_ENEMIGOS;
				//msj.name= ya esta el simbolo

				enviaMensaje(sockPlanif, &msj, sizeof(msj), logger,	"Se mato a alguien :P");
				break;
			}

			//Si pase las dos validaciones me muevo.
			getPosRecurso(list_items, msj.detail, &posRecX, &posRecY);

			// En base hacia donde se movio el personaje, calculo el movimiento
			switch (msj.detail2) {
			case ARRIBA:
				if (posY > 1)
					posY--;
				break;

			case ABAJO:
				if (posY < maxRows)
					posY++;
				break;

			case IZQUIERDA:
				if (posX > 1)
					posX--;
				break;
			case DERECHA:
				if (posX < maxCols)
					posX++;
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
				int cantInstancias = restarInstanciasRecurso(list_items,
						msj.detail);
				if (cantInstancias >= 0) {

					// Loqueo que al personaje se le dio un recurso
					log_info(logger, "Al personaje %c se le dio el recurso %c",
							personajeAux->simbolo, msj.detail);

					personajeAux->blocked = false;

					//msj.type = MOVIMIENTO; //El msj.type se setea cuando verifica pos de enemigos
					msj.detail2 = msj.detail;
					msj.detail = OTORGADO;
					msj.name = personajeAux->simbolo;

					// Envio mensaje donde confirmo la otorgacion del recurso pedido
					enviaMensaje(sockPlanif, &msj, sizeof(message_t), logger,
							"Se otorgo el recurso pedido");

				} else {

					// Logueo el bloqueo del personaje (que garron te bloqueaste puto)
					log_info(logger,
							"El personaje %c se bloqueo por el recurso %c",
							personajeAux->simbolo, msj.detail);

					//Lo pongo como bloqueado
					personajeAux->blocked = true;

					//msj.type = MOVIMIENTO; //El msj.type se setea cuando verifica pos de enemigos
					msj.detail2 = msj.detail;
					msj.detail = BLOCK;
					msj.name = personajeAux->simbolo;

					// Envio mensaje donde denego el pedido del recurso
					enviaMensaje(sockPlanif, &msj, sizeof(message_t), logger,
							"Se denego el pedido del recurso");

				}
			} else { //Si no llego al recurso sigue moviendose tranquilamente

				//msj.type = MOVIMIENTO; //El msj.type se setea cuando verifica pos de enemigos
				msj.detail2 = msj.detail;
				msj.detail = NADA;
				msj.name = personajeAux->simbolo;

				// Envio mensaje donde denego el pedido del recurso
				enviaMensaje(sockPlanif, &msj, sizeof(message_t), logger,
						"Se movio el personaje;");
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
					enviaMensaje(sockPlanif, &msj, sizeof(msj), logger,
							"Confirma salir al planificador");

					// Salgo del ciclo de recorrer personajes
					break;
				}

			}

			break;

		} //Fin del switch

		nivel_gui_dibujar(list_items, nom_nivel);
		pthread_mutex_unlock(&semNivel);

	}
	return 0;
}

void *enemigo(void * args) {

	//El numero de enemigos siempre empieza en 1 en adelante.

	//Si no hay personajes siempre me voy a mover en un cuadrado de 1x1

	//Armo estructura de enemigo que recibo por parametro cuando creo el hilo
	enemigo_t *enemigo;
	enemigo = (enemigo_t *) args;

	enemigo->posX = 0;
	enemigo->posY = 0;
	int contMovimiento = 1;
	int victimaX, victimaY;
	int currItem;
	int eje;
	ITEM_NIVEL *aux; //Aqui levanto cada item cuando los recorro

	//Obtengo mi posicion de enemigo
	getPosEnemy(list_items, enemigo->num_enemy, &(enemigo->posX),
			&(enemigo->posY));

	while (1) {

		if (list_size(list_personajes) == 0) {

			actualizaPosicion(&contMovimiento, &(enemigo->posX),
					&(enemigo->posY));

			if (enemigo->posX > maxCols || enemigo->posX < 0) {
				if (enemigo->posX >= maxCols)
					enemigo->posX -= 2;
				else
					enemigo->posX += 2;
			}
			if (enemigo->posY > maxRows || enemigo->posY < 0) {
				if (enemigo->posY > maxRows)
					enemigo->posY -= 2;
				else
					enemigo->posY += 2;
			}

			//Valido que no vaya al origeeeeeeeeeeeeen
			if (enemigo->posX <= 4 && enemigo->posY <= 4) {
				eje = rand() % 2;
				if (eje)
					enemigo->posX += 2;
				else
					enemigo->posY -= 2;
			}

			//**********************VALIDACION DE SI ESTOY ARRIBA DE UN RECURSO**********
			//Recorro la lista de items: comparo cada recurso con mi posicion
			for (currItem = 0; currItem < list_size(list_items); currItem++) {
				aux = (ITEM_NIVEL *) list_get(list_items, currItem);

				// Me fijo si es un recurso
				if (aux->item_type == RECURSO_ITEM_TYPE) {
					//Si luego de moverme estoy arriba de un recurso
					if (aux->posx == enemigo->posX
							&& aux->posy == enemigo->posY) {
						while (1) {
							enemigo->posX -= 2;
							enemigo->posY -= 2;
							//Si ahora estoy en una posicion libre de recursos salgo del ciclo. Sino sigo ciclando hasta moverme
							if (aux->posx != enemigo->posX
									|| aux->posy != enemigo->posY)
								break;
						}
						break;
					}
				}
			}
			//**********************VALIDACION DE SI ESTOY ARRIBA DE UN RECURSO**********
			pthread_mutex_lock(&semNivel);
			MoveEnemy(list_items, enemigo->num_enemy, enemigo->posX,
					enemigo->posY);

			nivel_gui_dibujar(list_items, nom_nivel);
			pthread_mutex_unlock(&semNivel);
			usleep(sleep_enemigos);

			if (contMovimiento == 4)
				contMovimiento = 1;
			else
				contMovimiento++;

			//Fin de if de turno

		} else { //Si hay personajes en el nivel tienes que perseguirlos: yo no dibujo, lo hace el nivel

			if (hayQueAsesinar) {

				int i;
				double *distancia = malloc(sizeof(int));
				pers_t *pjLevantador;
				char *victima_simb = malloc(sizeof(char));
				int posPerX;
				int posPerY;
				double distVictima = 0;
				mov_t *movimiento = malloc(sizeof(mov_t));
				movimiento->in_x = false;
				movimiento->in_y = false;
				//Recorro los personajaes conectado y elijo el que este mas cerca
				pthread_mutex_lock(&semNivel);
				for (i = 0; i < list_size(list_personajes); i++) {

					pjLevantador = (pers_t *) list_get_data(list_personajes, i);

					//Obtengo la posicion del personaje
					getPosPersonaje(list_items, pjLevantador->simbolo, &posPerX,
							&posPerY);

					//Calculo la distancia entre ambos
					*distancia = sqrt(
							pow((posPerX - enemigo->posX), 2)
									+ pow((posPerY - enemigo->posY), 2));

					if (i == 0 || distVictima >= *distancia) {
						*victima_simb = pjLevantador->simbolo;
						distVictima = *distancia;
						victimaX = posPerX;
						victimaY = posPerY;
					}

				}

				moverme(&victimaX, &victimaY, &(enemigo->posX),
						&(enemigo->posY), movimiento);

				//Valido que no vaya al origeeeeeeeeeeeeen
				if (enemigo->posX <= 4 && enemigo->posY <= 4) {
					if (movimiento->in_x)
						enemigo->posX += 2;
					if (movimiento->in_y)
						enemigo->posY -= 2;
				}

				validarPosSobreRecurso(list_items, *movimiento,
						&(enemigo->posX), &(enemigo->posY));

				MoveEnemy(list_items, enemigo->num_enemy, enemigo->posX,
						enemigo->posY);
				nivel_gui_dibujar(list_items, nom_nivel);

				if (hayAlgunEnemigoArriba(posPerX, posPerY)) {
					if (hayQueAsesinar) {
						KillPersonaje(list_personajes, *victima_simb); //Lo marca como muerto
						hayQueAsesinar = false;
						nivel_gui_dibujar(list_items, nom_nivel);
					}
				}
				pthread_mutex_unlock(&semNivel);
				free(victima_simb);
				free(distancia);
				free(movimiento);
				usleep(sleep_enemigos);

			} //Fin de if(hayQueAsesinar)
		} //Fin de else

	} //Fin de while(1)
	pthread_exit(NULL );

}

void KillPersonaje(t_list *list_personajes, char name) {
	int i;
	pers_t *pjAux;
	for (i = 0; i < list_size(list_personajes); i++) {
		pjAux = list_get(list_personajes, i);
		if (pjAux->simbolo == name)
			pjAux->muerto = true;
	}
}

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

void matar(enemigo_t *enemigo, pers_t *pjVictima, int indice,
		char*ip_plataforma, int puertoPlan) {
	message_t *msj = malloc(sizeof(message_t));
	msj->type = NIVEL;
	msj->detail = MUERTO_ENEMIGOS;
	msj->name = pjVictima->simbolo;

	liberarRecursos(pjVictima, indice);
	hayQueAsesinar = false;

	enemigo->sockP = connectServer(ip_plataforma, puertoPlan, logger,
			"planificador");

	enviaMensaje(enemigo->sockP, &msj, sizeof(msj), logger,
			"Informo asesinato al planificador");
	recibeMensaje(enemigo->sockP, &msj, sizeof(msj), logger,
			"Recibo confirmacion");
	free(msj);
}

bool hayAlgunEnemigoArriba(int posPerX, int posPerY) {

	int i;
	int posEnemyX, posEnemyY;
	for (i = 1; i <= cant_enemigos; i++) {

		getPosEnemy(list_items, i, &posEnemyX, &posEnemyY);
		if (posPerX == posEnemyX && posPerY == posEnemyY)
			return true;
	}
	return false;
}

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

void liberarRecursos(pers_t *personajeAux, int index_l_personajes) {

	int contRec;
	char* auxRec;
	// Si lo encontre recorro su lista de recursos y voy liberando lo que tenia
	for (contRec = 0; contRec < list_size(personajeAux->recursos); contRec++) {
		auxRec = (char *) list_get(personajeAux->recursos, contRec);
		sumarInstanciasRecurso(list_items, *auxRec);
	}

	//Destruyo la lista y los elementos de la lista: libera memoria
	list_destroy_and_destroy_elements(personajeAux->recursos,
			(void *) recurso_destroyer);

	if (personajeAux->muerto) { //Esto es porque si lo mato un enemigo que no intente borrarlo dos veces
		hayQueAsesinar = true;
		log_debug(logger, "El personaje %c esta muerto y reinicia",
				personajeAux->simbolo);
	}

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

void validarPosSobreRecurso(t_list *list_items, mov_t movimiento, int *posX,
		int *posY) {

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
