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
t_list *listaNiveles;
t_config *configPlataforma;
t_log *logger;
unsigned short usPuerto;
char *orqIp;
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

	// Defino la ip del orquestador, o sea este mismo proceso
	orqIp = malloc(sizeof(char) * 16);
	strcpy(orqIp, "127.0.0.1");

	orquestador(usPuerto);
//	pthread_create(&hilo_orquestador, NULL, orquestador, NULL );
//
//	pthread_join(hilo_orquestador, NULL );

	return EXIT_SUCCESS;
}

void *orquestador(unsigned short usPuerto) {
	t_list *lPlanificadores;
	pthread_t *pPlanificador;

	// Creo la lista de niveles y planificadores
	lPlanificadores = list_create();
	listaNiveles = list_create();

	// Definicion de variables para los sockets
	fd_set master;
	struct sockaddr_in myAddress;
	struct sockaddr_in remoteAddress;
	int maxSock;
	int sockListener;
	int socketComunicacion; // Aqui va a recibir el numero de socket que retornar getSockChanged()

	// Inicializacion de sockets y actualizacion del log
	iniSocks(&master, &myAddress, remoteAddress, &maxSock, &sockListener, usPuerto, logger);

	// Mensaje del orquestador
	tMensaje tipoMensaje;
	char * sPayload;

	while (1) {
		socketComunicacion = getConnection(&master, &maxSock, sockListener, &remoteAddress, &tipoMensaje, &sPayload, logger, "Orquestador");

		if (socketComunicacion != -1) {

			int iCantidadNiveles;

			pthread_mutex_lock(&semNivel);

				switch (tipoMensaje) {
				case SALUDO: // Un nuevo nivel se conecta

					if (!list_is_empty(listaNiveles)) {
						nivel_t *nNivelGuardado;
						int bEncontrado = 0;
						int iNivelLoop = 0;
						int iCantNiveles = list_size(listaNiveles);

						for (iNivelLoop = 0; (iNivelLoop < iCantNiveles) && (bEncontrado == 0); iNivelLoop++) {
							nNivelGuardado = (nivel_t *)list_get(listaNiveles, iNivelLoop);
							bEncontrado = (strcmp (nNivelGuardado->nombre, orqMsj.name) == 0);
						}

						if (bEncontrado == 1) { //TODO avisar a los q hacen pesonaje que traten este mensaje
							orqMsj.type = REPETIDO;
							enviarPaquete(socketComunicacion, &orqMsj, logger, "Ya se encuentra conectado al orquestador un nivel con el mismo nombre");
							break;
						}
					}

					nivel_t *nivelNuevo = (nivel_t *) malloc(sizeof(nivel_t));
					pPlanificador = (pthread_t *) malloc(sizeof(pthread_t));
					log_debug(logger, "SALUDO: se conecto el nivel %s", orqMsj.name);
					char * name_auxi = strdup(orqMsj.name);

					// Ahora debo esperar a que me llegue la informacion de planificacion.
					recibirPaquete(socketComunicacion, &tipoMensaje, &sPayload, logger, "Recibe mensaje INFO de planificacion");

					// Validacion de que el nivel me envia informacion correcta
					if (orqMsj.type == INFO) {

						create_level(nivelNuevo, socketComunicacion, name_auxi, orqMsj.name, orqMsj.detail, orqMsj.port);

						crearHiloPlanificador(pPlanificador, nivelNuevo, lPlanificadores);

						inicializarConexion(&nivelNuevo->masterfds, &nivelNuevo->maxSock, &socketComunicacion);

						delegarConexion(&nivelNuevo->masterfds, &master, &socketComunicacion, &nivelNuevo->maxSock);

						// Logueo el nuevo hilo recien creado
						log_debug(logger, "Nuevo planificador del nivel: '%s' y planifica con: %s", nivelNuevo->nombre, nivelNuevo->algoritmo);


					} else {
						log_error(logger,"Tipo de mensaje incorrecto: se esperaba INFO del nivel");
						exit(EXIT_FAILURE);
					}
					break;
				break;

				case SALUDO:

					iCantidadNiveles = list_size(listaNiveles);

					if (iCantidadNiveles == 0) {
						orqMsj.detail = NADA;
						enviarPaquete(socketComunicacion, &orqMsj, logger, "No hay niveles conectados a la plataforma");
						break;
					}

					nivel_t * nivel_pedido = search_nivel_by_name_with_return(orqMsj.name);

					if(existeNivel(nivel_pedido)){
						// Logueo del pedido de nivel del personaje
						log_trace(logger, "Se conectó el personaje: %c en sock %d, Pide nivel: %s",orqMsj.ip[0], socketComunicacion, orqMsj.name);

						// Armo mensaje SALUDO para el personaje con la info del planificador
						orqMsj.detail = SALUDO;

						delegarConexion(&nivel_pedido->masterfds, &master, &socketComunicacion, &nivel_pedido->maxSock);

						signal_personajes(&nivel_pedido->hay_personajes);

						// Le aviso al personaje sobre el planificador a donde se va a conectar
						enviarPaquete(socketComunicacion, &orqMsj, logger, "Envio al personaje la info del planificador del nivel");

					} else {
						// El nivel solicitado no se encuentra conectado todavia
						orqMsj.detail = NADA;
						enviarPaquete(socketComunicacion, &orqMsj, logger, "No se encontro el nivel pedido");
					}

					break; //break del case SALUDO dentro del personaje
				}

			pthread_mutex_unlock(&semNivel);
		}//Fin del if
	}//Fin del while

	return NULL ;
}

void* planificador(void *argumentos) {

	// Armo el nivel que planifico con los parametros que recibe el hilo
	nivel_t *nivel;
	nivel = (nivel_t*) argumentos;

	//Para multiplexar
	int iSocketConexion;
	fd_set readfds;
	FD_ZERO(&readfds);
	message_t msj;

	int indice_personaje = 0; //Esto es para guardar un indice de c/personaje
	int contPj; //Esto es para recorrer las listas de personajes
	int proxPj = 0;
	int q = 1;
	int Quantum = nivel->Quantum; //Esta variable se cambia solo cuando se cambia el quantum en el archivo de configuracion del nivel
	personaje_t personajeNuevo;
	personaje_t *pjLevantador = malloc(sizeof(personaje_t));

	turno_t reMultiplex, primerPj, casiListo;
	reMultiplex.condition = false;
	primerPj.condition = false;
	casiListo.condition = false;

	_Bool first_time = true;

	// Ciclo donde se multiplexa para escuchar personajes que se conectan
	while (1) {

		wait_personajes(&nivel->hay_personajes);

		iSocketConexion = multiplexar(&nivel->masterfds, &readfds, &nivel->maxSock, &msj, sizeof(msj), logger);

		if (iSocketConexion != -1) {

			bool encontrado;

			pthread_mutex_lock(&semNivel);
			//Esto lo pongo pero esta al pedo porque siempre va a recibir del personaje
			switch (msj.type) {

			case PERSONAJE:
				switch (msj.detail) {
				case SALUDO:

					//Me fijo si esta en la cola de finalizados
					if (exist_personaje(nivel->l_personajesDie, msj.name, &contPj)) {

						log_trace(logger, "Se conecto personaje %c en socket %d", msj.name,  iSocketConexion);

						//Lo saco de finalizados y lo mando a ready
						pjLevantador = list_remove(nivel->l_personajesDie, contPj);
						//Actualizo su socket
						pjLevantador->sockID = iSocketConexion;
						//No hace falta pero lo vuelvo a marcar como muerto por las dudas
						pjLevantador->kill   = true;
						pjLevantador->ready  = false;
						list_add(nivel->l_personajesRdy, pjLevantador);

						reMultiplex.condition = false;

						msj.type = SALUDO;
						enviarPaquete(nivel->sock, &msj, logger, "Saludo Nivel");
						recibirPaquete(nivel->sock, &tipoMensaje, &sPayload, logger, "Recibe SALUDO del nivel con pos inicial");

						msj.type    = SALUDO;
						//msj.detail  = pos inicial x
						//msj.detail2 = pos inicial y
						enviarPaquete(iSocketConexion, &msj, logger,"Envio SALUDO al personaje");

						// Logueo la modificacion de la cola y notifico por pantalla
						log_trace(logger,"Se agrego el personaje %c a la cola de listos",pjLevantador->name);
						imprimirLista(nivel->nombre,nivel->l_personajesRdy,nivel->l_personajesBlk, proxPj);

					} else {

						log_trace(logger, "Se conecto personaje %c en socket %d", msj.name,  iSocketConexion);

						create_personaje(nivel->l_personajesRdy, msj.name, iSocketConexion, indice_personaje++);

						//Envio el turno al primer personaje
						if (first_time || (!first_time && list_size(nivel->l_personajesRdy) == 0)) {
							first_time = false;
							setTurno(&primerPj, true, iSocketConexion);
						}

						reMultiplex.condition = false;

						msj.type = SALUDO;
						enviarPaquete(nivel->sock, &msj, logger, "Saludo Nivel");

						recibirPaquete(nivel->sock, &tipoMensaje, &sPayload, logger, "Recibe SALUDO del nivel con pos inicial");
						msj.type    = SALUDO;
						//msj.detail  = pos inicial x
						//msj.detail2 = pos inicial y

						// Envio la informacion de posicion al personaje
						enviarPaquete(iSocketConexion, &msj, logger,"Envio SALUDO al personaje");

						// Logueo la modificacion de la cola y notifico por pantalla
						log_trace(logger,"Se agrego el personaje %c a la cola de listos",personajeNuevo.name);
						imprimirLista(nivel->nombre,nivel->l_personajesRdy,nivel->l_personajesBlk, proxPj);
					}
					break;

				case POSICION_RECURSO:

					// Armo mensaje a enviar al nivel
					msj.type = POSICION_RECURSO;

					// Envio mensaje al nivel con la informacion
					enviarPaquete(nivel->sock, &msj, logger, "Solicito posicion de recurso al nivel");

					// Recibo informacion de la posicion del recurso
					recibirPaquete(nivel->sock, &tipoMensaje, &sPayload, logger, "Recibo posicion de recurso del nivel");

					//Armo mensaje para enviar al personaje
					msj.type = POSICION_RECURSO;
					//msj.detail=pos en X
					//msj.detail2= pos en Y
					//msj.name = simbolo

					// Envio la informacion al personaje
					enviarPaquete(iSocketConexion, &msj, logger, "Envio posicion del recurso");

					//Si soy el primer personaje que entra al planificador, entonces le doy un turno
					if (primerPj.condition) {
						pjLevantador = search_pj_by_socket_with_return(nivel->l_personajesRdy, primerPj.sock);
						turno(&primerPj, nivel->delay, "Turno para el primer personaje");
						log_trace(logger, "%s: Turno(primerPersonaje) para %c con quantum=%d",nivel->nombre, pjLevantador->name, q);
						break;
					}

					marcarPersonajeComoReady(nivel->l_personajesRdy, iSocketConexion);

					if (pjLevantador->kill) {
						pjLevantador->kill = false;
						setTurno(&reMultiplex, true, iSocketConexion);
					}

					//Si tengo que volver a multiplexar: esta variable solo la pongo en true en el case de MOVIMIENTO
					if (reMultiplex.condition) {
						pjLevantador = search_pj_by_socket_with_return(nivel->l_personajesRdy, reMultiplex.sock);
						q++;
						turno(&reMultiplex, nivel->delay, "Envia proximo turno");
						log_trace(logger, "%s: Turno(reMultiplexar) para %c con quantum=%d",nivel->nombre, pjLevantador->name, q);
						break;
					}

					if (casiListo.condition && casiListo.sock == iSocketConexion) {
						pjLevantador = search_pj_by_socket_with_return(nivel->l_personajesRdy, casiListo.sock);
						turno(&casiListo, nivel->delay, "Envia proximo turno");
						log_trace(logger, "%s: Turno(pj_casi_listo) para %c con quantum=%d",nivel->nombre, pjLevantador->name, q);
						break;
					}

					reMultiplex.condition = false;

					break;
				case TURNO:

					//Si no lo mato un enemigo, le doy turnos normalmente

					//Si hay personajes en la cola de listos: entonces debo darle un turno a alguno de ellos
					if (list_size(nivel->l_personajesRdy) != 0) {
						//Para darles un turno debo actualizar su quantum

						//--Si ya terminó su quantum debo pasar al proximo personaje
						if (q >= Quantum) {
							//Avanzo al proximo personaje
							proxPj++;

							if (proxPj== list_size(nivel->l_personajesRdy))
								proxPj = 0;

							q = 1;
						} else {
							q++;
						}
						//Uso la variable proxPj para sacar el personaje al que debo darle el turno

						pjLevantador = (personaje_t *) list_get_data(nivel->l_personajesRdy, proxPj);

						//Esta parte es para cuando quiera mandar un turno nates de que haya recibido la posicion
						//Esto valida si hay un personaje que todavia no haya recibido la pos de su recuros y que no le mande un turno ahi
						//Ademas va a ser siempre un personaje nuevo a ejecutar y por lo tanto con q=1
						if ((list_size(nivel->l_personajesRdy)) >1 && (q==1) && !pjLevantador->ready) {
							setTurno(&casiListo, true, pjLevantador->sockID);
							break;
						}

						msj.type   = PERSONAJE;
						msj.detail = TURNO;

						log_trace(logger, "%s: Turno(TURNO) para %c con quantum=%d",nivel->nombre, pjLevantador->name, q);

						usleep(nivel->delay);
						enviarPaquete(pjLevantador->sockID, &msj, logger,"Envia proximo turno");

					} else {
						log_error(logger, "Me quedo en cero: %d",list_size(nivel->l_personajesRdy));
					}

					reMultiplex.condition = false;
					break;

				case MOVIMIENTO:

					pjLevantador = search_pj_by_socket_with_return(nivel->l_personajesRdy, iSocketConexion);

					msj.type = MOVIMIENTO;
					msj.detail = msj.name;
					//msjPlan.detail2 = calculaMovimiento
					msj.name = pjLevantador->name;

					// Le envio al nivel el movimiento del personaje
					enviarPaquete(nivel->sock, &msj, logger, "Envio movimiento del personaje");

					recibirPaquete(nivel->sock, &tipoMensaje, &sPayload, logger, "Recibo estado en el que quedo el personaje");

					if (msj.detail == MUERTO_ENEMIGOS) {

						encontrado = sacarPersonajeDeListas(nivel->l_personajesRdy, nivel->l_personajesBlk,
														iSocketConexion, pjLevantador);

						desbloquearPersonajes(nivel->l_personajesBlk, nivel->l_personajesRdy,
												pjLevantador, encontrado, nivel->nombre, proxPj);

						msj.type = SALIR;
						msj.name = pjLevantador->name;

						enviarPaquete(nivel->sock, &msj, logger,"Salida del nivel");

						recibirPaquete(nivel->sock, &tipoMensaje, &sPayload, logger,"Recibe confirmacion del nivel");

						//Limpio la lista de recursos que tenia hasta que se murio
						list_clean(pjLevantador->recursos);
						//Lo marco como muerto y lo desmarco cuando pide POSICION_RECURSO
						pjLevantador->kill = true;
						//Lo agrego a la cola de finalizados y lo saco cuando manda msj en el case SALUDO
						list_add(nivel->l_personajesDie, pjLevantador);

						msj.type=MOVIMIENTO;
						msj.detail = MUERTO_ENEMIGOS;
						enviarPaquete(iSocketConexion, &msj, logger, "El personaje ha perdido una vida por enemigos");

						if (list_size(nivel->l_personajesRdy) > 0) {
							proxPj--;

							if (list_size(nivel->l_personajesRdy) == proxPj || proxPj<0) {
								proxPj = 0;
							}

							q = 1;
						}

						imprimirLista(nivel->nombre, nivel->l_personajesRdy, nivel->l_personajesBlk, proxPj);

						break;
					}

					if (msj.detail != NADA) {

						//Agregar recurso pedido a su lista de recursos: siempre se lo agregamos, este o nó bloqueado
						list_add_new(pjLevantador->recursos, &(msj.name),sizeof(msj.name));

						if (msj.detail == BLOCK) { //Si volvió bloqueado

							if (msj.name != pjLevantador->name) {
								log_warning(logger,	"msj.name: %c  DISTINTO QUE pjLevantador.name: %c",	msj.name, pjLevantador->name);
								exit(EXIT_FAILURE);
							}

							log_info(logger,"Personaje: %c esta bloqueado por: %c",pjLevantador->name, msj.detail2);

							//Lo saco de LISTOS y lo paso a BLOQUEADOS
							list_add(nivel->l_personajesBlk,(personaje_t*) list_remove(nivel->l_personajesRdy,contPj));

							//Acabo de sacar a un personaje en la cola de listos, entonces el proxPj "apunta" a un lugar donde ya no esta ese personaje
							//Entonces lo resto
							proxPj--;

							imprimirLista(nivel->nombre, nivel->l_personajesRdy, nivel->l_personajesBlk, (contPj - 1));

							//Si bien quedo bloqueado, igual tengo que aumentarle su quantum
							q = Quantum + 1;

						} else {//Si se le otorgo el recurso

							log_info(logger, "Personaje: %c se le otorgo: %c",pjLevantador->name, msj.detail2);

							//Pongo esto para volver a multiplexar porque sino no da mas turnos: tal vez haya que sacar esto mas adelante. TODO
							if (list_size(nivel->l_personajesRdy) > 0) {
								setTurno(&reMultiplex, true, iSocketConexion);
								pjLevantador->ready = false;
								casiListo.condition = false;
							}
						}

					} else { //Si no habia llegado al recurso y solo se habia movido
						log_info(logger, "Personaje: %c se movio",pjLevantador->name);
						pjLevantador->ready = true;
					}

					if (msj.detail!=BLOCK) {
						msj.type = MOVIMIENTO;
						msj.detail=NADA;
						enviarPaquete(iSocketConexion, &msj, logger, "El personaje se movio");
					}

					break;

				case SALIR: //En el case SALIR principalmente Libero memoria y recursos en el nivel

					if (msj.detail2 != MUERTO_ENEMIGOS) {
						//El personaje se quedo sin vidas pero ya devolvio recursos en el case de MOVIMIENTO
						//por lo tanto tiene que liberar memoria solamente y volver a multiplexar

						encontrado = false;

						encontrado = sacarPersonajeDeListas(nivel->l_personajesRdy, nivel->l_personajesBlk,
														iSocketConexion, pjLevantador);

						desbloquearPersonajes(nivel->l_personajesBlk, nivel->l_personajesRdy,
												pjLevantador, encontrado, nivel->nombre, proxPj);

						msj.type = SALIR;
						msj.name = pjLevantador->name;

						enviarPaquete(nivel->sock, &msj, logger,"Salida del nivel");

						recibirPaquete(nivel->sock, &tipoMensaje, &sPayload, logger,"Recibe confirmacion del nivel");

						//Limpio la lista de recursos del personaje
						list_clean(pjLevantador->recursos);

						encontrado = true; //La pongo en true para marcar que saco un personaje de ready: modifico proxPj

					} else {

						pjLevantador = search_pj_by_socket_with_return(nivel->l_personajesDie, iSocketConexion);

						encontrado = false; //Saco un personaje de die, no de ready: no modifico proxPj
					}

					msj.type = SALIR;
					msj.detail = EXIT_SUCCESS;
					char *msjInfo = malloc(sizeof(char) * 100);
					sprintf(msjInfo, "Confirmacion de Salir al personaje %c", pjLevantador->name);
					enviarPaquete(iSocketConexion, &msj, logger, msjInfo);

					//Libero memoria
					free(msjInfo);
					free(pjLevantador->recursos);
					free(pjLevantador);

					//Forzamos otro mensaje de turno para volver a multiplexar
					if (list_size(nivel->l_personajesRdy) > 0) {

						proxPj--;

						if (list_size(nivel->l_personajesRdy) == proxPj || proxPj<0) {
							proxPj = 0;
						}

						q = 1;

						personaje_t *pjAux = (personaje_t *) list_get(nivel->l_personajesRdy, proxPj);

						imprimirLista(nivel->nombre, nivel->l_personajesRdy,
								nivel->l_personajesBlk, proxPj);

						log_trace(logger, "%s: Turno(salir) para %c con quantum=%d",nivel->nombre, pjAux->name, q);

						//--Cachear pj
						msj.detail = TURNO;
						usleep(nivel->delay);
						enviarPaquete(pjAux->sockID, &msj, logger, "Forzado de turno");//TODO aqui tiene un bug

					}
					log_debug(logger, "Libere recursos");
					break; //Fin del case SALIR

				} //Fin de switch(msj.detail)

				break; //Fin del case PERSONAJE

			case NIVEL: //Este case solo se ejecuta la primera vez que envia un mensaje el nivel cuando apenas se conecta al planificador
				switch (msj.detail) {
				case INFO: //TODO actualizar campos que me mandan
					log_debug(logger, "Vergaaaaaaaaa");
					break;
				}

				break;

			} //Fin de switch(msj.type)
			pthread_mutex_unlock(&semNivel);

		} //Fin de if(i!=-1)


	} //Fin de while(1)

	return NULL ;
}

void create_level(nivel_t * nivelNuevo, int sock, char *level_name, char *alg, int q, int delay){
	strcpy(nivelNuevo->nombre, level_name);
	strcpy(nivelNuevo->algoritmo, alg);
	nivelNuevo->l_personajesBlk = list_create();
	nivelNuevo->l_personajesRdy = list_create();
	nivelNuevo->l_personajesDie = list_create();
	nivelNuevo->sock = sock;
	nivelNuevo->Quantum = q;
	nivelNuevo->delay = delay;
	nivelNuevo->maxSock = 0;

	list_add(listaNiveles, nivelNuevo);
}

void create_personaje(t_list *lista, char simbolo, int sock, int indice){
	personaje_t personajeNuevo;
	personajeNuevo.name   = simbolo;
	personajeNuevo.sockID = sock;
	personajeNuevo.ready  = false;
	personajeNuevo.kill   = false;
	personajeNuevo.index = indice;
	personajeNuevo.recursos = list_create();
	list_add_new(lista, (void *)&personajeNuevo, sizeof(personaje_t));
}

void crearHiloPlanificador(pthread_t *pPlanificador, nivel_t *nivelNuevo, t_list *list_p){
	if (pthread_create(pPlanificador, NULL, planificador, (void *)nivelNuevo)) {
		log_error(logger, "pthread_create: %s",strerror(errno));
		exit(EXIT_FAILURE);
	}
	list_add(list_p, pPlanificador);
}

nivel_t * search_nivel_by_name_with_return(char *level_name){
	int i;
	for(i = 0; i<list_size(listaNiveles); i++){
		nivel_t* nivel = (nivel_t*) list_get(listaNiveles, i);
		if(string_equals_ignore_case(nivel->nombre, level_name))
			return nivel;
	}
	return NULL;
}

bool existeNivel(nivel_t *nivel){
	return (nivel!=NULL);
}

void marcarPersonajeComoReady(t_list *ready, int sock){
	personaje_t *pjAuxi = search_pj_by_socket_with_return(ready, sock);
	pjAuxi->ready = true;
}

personaje_t *search_pj_by_name_with_return(t_list *lista, char name){
	int contPj;
	personaje_t *pjLevantador;
	for(contPj =0 ; contPj<list_size(lista); contPj++){
		pjLevantador = list_get(lista, contPj);
		if(pjLevantador->name == name)
			return pjLevantador;
	}
	return NULL;
}


personaje_t *search_pj_by_socket_with_return(t_list *lista, int sock){
	personaje_t * personaje;
	int contPj;
	for(contPj =0 ; contPj<list_size(lista); contPj++){
		personaje = list_get(lista, contPj);
		if(personaje->sockID == sock)
			return personaje;
	}
	return NULL;
}

void search_pj_by_socket(t_list *lista, int sock, personaje_t *personaje){
	int contPj;
	for(contPj =0 ; contPj<list_size(lista); contPj++){
		personaje = list_get(lista, contPj);
		if(personaje->sockID == sock)
			break;
	}
}

void search_pj_by_name(t_list *lista, char name, personaje_t *personaje){
	int contPj;
	for(contPj =0 ; contPj<list_size(lista); contPj++){
		personaje = list_get(lista, contPj);
		if(personaje->name == name)
			break;
	}
	personaje = NULL;
}

int search_index_of_pj_by_name(t_list *lista, char name, personaje_t *personaje){
	int contPj;
	for(contPj =0 ; contPj<list_size(lista); contPj++){
		personaje = list_get(lista, contPj);
		if(personaje->name == name)
			return contPj;
	}
	return -1;
}

int search_index_of_pj_by_socket(t_list *lista, int sock, personaje_t *personaje){
	int contPj;
	for(contPj =0 ; contPj<list_size(lista); contPj++){
		personaje = list_get(lista, contPj);
		if(personaje->sockID == sock)
			return contPj;
	}
	return -1;
}


void turno(turno_t *turno, int delay, char *msjInfo){
	message_t msj;
	armarMsj(&msj, 0, 0, TURNO, 0);
	turno->condition=false;
	usleep(delay);
	enviarPaquete(turno->sock, &msj, logger, msjInfo);
}

void setTurno(turno_t *turno, bool condition_changed, int sock){
	turno->condition = condition_changed;
	if(sock != 0)
		turno->sock = sock;
}

void desbloquearPersonajes(t_list* block, t_list *ready, personaje_t *pjLevantador, bool encontrado, char*nom_nivel, int proxPj){
	int contRec, j;
	personaje_t* pjLevantadorBlk;
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
					enviarPaquete(pjLevantadorBlk->sockID, &msj, logger,"Se desbloqueo un personaje");

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
	personaje_t * pPersonaje;

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
		personaje_t * pjAux = list_get(end, iIndice);

		if (pjAux->name == name) {
			return true;
		}
	}

	return false;
}

bool sacarPersonajeDeListas(t_list *ready, t_list *block, int sock,  personaje_t *pjLevantador) {

	int indice_personaje;
	bool encontrado = false;

	indice_personaje = search_index_of_pj_by_socket(block, sock, pjLevantador);

	if(indice_personaje != -1){
		encontrado = true; //Si lo encontras en Blk, cambiamos el flag
		pjLevantador = list_remove(block, indice_personaje); // Y sacamos al personaje de la lista de bloqueados
	}
	//Si no lo encuentra: lo busco en la lista de Ready y lo saco de la misma
	if (!encontrado) { //Si no lo encontras, buscarlo en rdy
		indice_personaje = search_index_of_pj_by_socket(ready, sock, pjLevantador);
		pjLevantador = list_remove(ready, indice_personaje); // Lo sacamos de la lista
	}

	return encontrado;
}

void imprimirLista(char* nivel, t_list* rdy, t_list* blk, int cont) {

	char* tmp = malloc(20);
	char* retorno = malloc(500);
	int i;

	personaje_t* levantador;

	if (list_size(rdy) == 0 || cont < 0) { //Si no hay nadie listo, no se quien esta ejecutando
		sprintf(retorno, "Lista de: %s\n\tEjecutando:\n\tListos: \t", nivel);
	} else {
		levantador = (personaje_t *)list_get_data(rdy, cont);
		sprintf(retorno, "Lista de: %s\n\tEjecutando: %c\n\tListos: \t", nivel, levantador->name);
	}

	for (i = 0; i < list_size(rdy); i++) {
		levantador = (personaje_t *)list_get_data(rdy, i);
		sprintf(tmp, "%c -> ", levantador->name);
		string_append(&retorno, tmp);
	}

	sprintf(tmp, "\n\tBloqueados: \t");
	string_append(&retorno, tmp);

	for (i = 0; i < list_size(blk); i++) {
		levantador = list_get(blk, i);
		sprintf(tmp, "%c -> ", levantador->name);
		string_append(&retorno, tmp);
	}

	log_info(logger, retorno);
	free(tmp);
	free(retorno);
}

void delegarConexion(fd_set *master_planif, fd_set *master_orq, int *sock, int *maxSock){
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

void inicializarConexion(fd_set *master_planif, int *maxSock, int *sock){
	FD_ZERO(master_planif);
	*maxSock = *sock;
}

void imprimirConexiones(fd_set *master_planif, int maxSock, char* host){
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

void signal_personajes(bool *hay_personajes){
	*hay_personajes = true;
	pthread_cond_signal(&hayPersonajes);
}

void wait_personajes(bool *hay_personajes){
	if(!(*hay_personajes)){
		pthread_mutex_lock(&semNivel);
		pthread_cond_wait(&hayPersonajes, &semNivel);
		pthread_mutex_unlock(&semNivel);
	}
}

