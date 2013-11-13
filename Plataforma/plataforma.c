/*
 * Sistemas Operativos - Super Mario Proc RELOADED.
 * Grupo       : C o no ser.
 * Nombre      : plataforma.c.
 * Descripcion : Este archivo contiene la implementacion de las
 * funciones usadas por la plataforma.
 */

#include "plataforma.h"

pthread_mutex_t semNivel;
pthread_t hilo_orquestador;
t_list *listaNiveles;
t_config *configPlataforma;
t_log *logger;
unsigned short usPuerto;
char *orqIp;
char *pathKoopa;
char *pathScript;

int main(int argc, char*argv[]) {

	if (argc <= 1) {
		printf("[ERROR] Se debe pasar como parametro el archivo de configuracion.\n");
		exit(EXIT_FAILURE);
	}

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

	// Defino la ip del orquestador, o sea este mismo proceso
	orqIp = malloc(sizeof(char) * 16);
	strcpy(orqIp, "127.0.0.1");

	orquestador(usPuerto);
//	pthread_create(&hilo_orquestador, NULL, orquestador, NULL );
//
//	pthread_join(hilo_orquestador, NULL );

	// TODO validar que los personajes terminaron sus niveles

	// TODO ejecutar el binario koopa junto con el script para el filesystem

	return EXIT_SUCCESS;
}

void *orquestador(unsigned short usPuerto) {
	t_list *lPlanificadores;
	threadPlanificador_t *pPlanificador;

	// Creo la lista de niveles y planificadores
	listaNiveles    = list_create();
	lPlanificadores = list_create();

	// Defino un puerto para el planificador, luego cuando tire cada hilo planificador
	// voy a ir aumentando el numero para que cada uno tenga un puerto diferente
	int lastPlan = PUERTO_PLAN;

	// Definicion de variables para los sockets
	fd_set master;
	fd_set temp;
	struct sockaddr_in myAddress;
	struct sockaddr_in remoteAddress;
	int maxSock;
	int sockListener;
	int socketComunicacion; // Aqui va a recibir el numero de socket que retornar getSockChanged()

	// Inicializacion de sockets y actualizacion del log
	iniSocks(&master, &temp, &myAddress, remoteAddress, &maxSock, &sockListener, usPuerto, logger);

	// Mensaje del orquestador
	orq_t orqMsj;

	while (1) {
		socketComunicacion = getSockChanged(&master, &temp, &maxSock, sockListener, &remoteAddress, &orqMsj, sizeof(orq_t), logger, "Orquestador");

		if (socketComunicacion != -1) {

			bool nivelEncontrado;

			pthread_mutex_lock(&semNivel);

			switch (orqMsj.type) {
			case NIVEL:
				switch (orqMsj.detail) {
				case SALUDO:
					// Un nuevo nivel se conecta

					// Se verifica que no exista uno con le mismo nombre
					if (!list_is_empty(lPlanificadores)) {
						threadPlanificador_t *pPlanifGuardado;
						int bEncontrado = 0;
						int iPlanificadorLoop = 0;
						int iCantPlanificadores = list_size(lPlanificadores);

						for (iPlanificadorLoop = 0; (iPlanificadorLoop < iCantPlanificadores) && (bEncontrado == 0); iPlanificadorLoop++) {
							pPlanifGuardado = list_get(lPlanificadores, iPlanificadorLoop);
							bEncontrado = (strcmp (pPlanifGuardado->nivel.nombre, orqMsj.name) == 0);
						}

						if (bEncontrado == 1) {
							orqMsj.type = REPETIDO;
							enviaMensaje(socketComunicacion, &orqMsj, sizeof(orq_t), logger, "Ya se encuentra conectado al orquestador un nivel con el mismo nombre");
							break;
						}
					}

					pPlanificador = (threadPlanificador_t*) malloc(sizeof(threadPlanificador_t));
					// Logueo la conexion del nivel
					log_debug(logger, "Recibe msj SALUDO de %s", orqMsj.name);

					// Armo estructura para ese nuevo nivel disponible y lo agrego a la lista de niveles
					pPlanificador->nivel.sock = socketComunicacion;
					pPlanificador->nivel.puertoPlan = lastPlan; //Puerto del planificador asociado
					strcpy(pPlanificador->nivel.nombre, orqMsj.name);

					// Cada nivel va a tener su cola de bloqueados, listos y finalizados
					pPlanificador->nivel.l_personajesBlk = list_create();
					pPlanificador->nivel.l_personajesRdy = list_create();
					pPlanificador->nivel.l_personajesDie = list_create();

					// Logueo la conexion del nivel
					log_trace(logger, "Se conectó el nivel: %s", orqMsj.name);

					// Ahora debo esperar a que me llegue la informacion de planificacion.
					recibeMensaje(socketComunicacion, &orqMsj, sizeof(orq_t), logger, "Recibe mensaje INFO de planificacion");

					// Validacion de que el nivel me envia informacion correcta
					if (orqMsj.type == INFO) {

						// Asigno los valores a las variables del nivel
						strcpy(pPlanificador->nivel.algoritmo,orqMsj.name);
						pPlanificador->nivel.quantum = orqMsj.detail;
						pPlanificador->nivel.retardo = orqMsj.port;

						// Agrego el nivel a la lista de niveles
						list_add_new(listaNiveles, (void *) &pPlanificador->nivel, sizeof(nivel_t));

						// Tira el nuevo hilo; valido errores
						if (pthread_create(&pPlanificador->hiloPlan, NULL, planificador, (void *) &pPlanificador->nivel)) {

							// Mandamos al hilo t0do el nivel
							log_error(logger, "pthread_create: %s",strerror(errno));
							exit(EXIT_FAILURE);
						}

						// Logueo el nuevo hilo recien creado
						log_debug(logger, "Nuevo planificador del nivel: '%s' que atiende puerto: %d y planifica con: %s",
								pPlanificador->nivel.nombre, lastPlan,
								pPlanificador->nivel.algoritmo);

						//Envio el puerto e ip del planificador para que el nivel haga el connect
						orqMsj.type = INFO_PLANIFICADOR;
						orqMsj.port = lastPlan;
						strcpy(orqMsj.ip, orqIp);

						enviaMensaje(socketComunicacion, &orqMsj, sizeof(orq_t), logger, "Envio de puerto e ip del planificador al nivel");

						//Luego de crear el hilo aumento en uno el contador de hilos para el siguiente planificador
						list_add(lPlanificadores, pPlanificador);

						// Incremento el puerto
						lastPlan = lastPlan + 15;

					} else {
						log_error(logger,"Tipo de mensaje incorrecto: se esperaba INFO del nivel");
						exit(EXIT_FAILURE);
					}

					break;
				}

				break;

			case PERSONAJE:
				// Un nuevo personaje se conecta
				switch (orqMsj.detail) {
				case SALUDO:

					nivelEncontrado = false;
					// El personaje pide jugar
					// Ciclo para verificar los niveles que tiene que hacer el personaje
					int iCantidadNiveles = list_size(listaNiveles);

					if (iCantidadNiveles == 0) {
						orqMsj.detail = NADA;
						enviaMensaje(socketComunicacion, &orqMsj, sizeof(orq_t), logger, "No hay niveles conectados a la plataforma");
						break;
					}

					int iNivelLoop;

					for (iNivelLoop = 0; iNivelLoop <= iCantidadNiveles; iNivelLoop++) {

						nivel_t* nivelLevantador = (nivel_t*) list_get_data(listaNiveles, iNivelLoop);

						// Validacion del nivel encontrado
						if (string_equals_ignore_case(nivelLevantador->nombre, orqMsj.name)) {

							nivelEncontrado = true;

							// Logueo del pedido de nivel del personaje
							log_trace(logger, "Se conectó el personaje: %c en sock %d, Pide nivel: %s",orqMsj.ip[0], socketComunicacion, orqMsj.name);

							// Armo mensaje SALUDO para el personaje con la info del planificador
							orqMsj.detail = SALUDO;
							orqMsj.port = nivelLevantador->puertoPlan;
							strcpy(orqMsj.ip, orqIp);

							// Le aviso al personaje sobre el planificador a donde se va a conectar
							enviaMensaje(socketComunicacion, &orqMsj, sizeof(orq_t), logger, "Envio al personaje la info del planificador del nivel");

							break;

						} //Fin del if

					} //Fin del for

					if (nivelEncontrado == false) {
						// El nivel solicitado no se encuentra conectado todavia
						orqMsj.detail = NADA;
						enviaMensaje(socketComunicacion, &orqMsj, sizeof(orq_t), logger, "No se encontro el nivel pedido");
					}

					break; //break del case SALUDO dentro del personaje
				}
				break; //break del case PERSONAJE
			}//Fin del switch
			pthread_mutex_unlock(&semNivel);
		}//Fin del if
	}//Fin del while

	return NULL ;
}

void* planificador(void *argumentos) {

	// Armo el nivel que planifico con los parametros que recibe el hilo
	nivel_t *nivelPlanif;
	nivelPlanif = (nivel_t*) argumentos;

	// Definicion de variables para los sockets donde se va a escuchar
	fd_set master;
	fd_set temp;
	struct sockaddr_in myAddress;
	struct sockaddr_in remoteAddress;
	int maxSock;
	int sockListener;
	int iSocketConexion; // Aqui va a recibir el numero de socket que retornar getSockChanged()

	// Inicializacion de sockets y actualizacion del log
	iniSocks(&master, &temp, &myAddress, remoteAddress, &maxSock, &sockListener, nivelPlanif->puertoPlan, logger);

	// Mensajes que llegan . Ahora puede ser un nivel o un personaje el socket 'i'
	message_t msj;

	int contPersonajes = 0; //Esto es para guardar un indice de c/personaje
	int contPj; //Esto es para recorrer las listas de personajes
	int proxPj = 0;
	int q = 1;
	int Quantum = nivelPlanif->quantum; //Esta variable es de solo lectura
	personaje_t personajeNuevo;
	personaje_t *pjLevantador = malloc(sizeof(personaje_t));

	_Bool reMultiplexar = false;
	int soyElPrimerPersonajeDameUnTurno = 0;
	int sockReMultiplexar;
	int sockPrimerPersonaje;
	_Bool first_time = true;
	int sock_pj_casi_listo;
	_Bool dar_turno_al_pj_casi_listo = false;

	// Ciclo donde se multiplexa para escuchar personajes que se conectan
	while (1) {
		// Multiplexo
		iSocketConexion = getSockChanged(&master, &temp, &maxSock, sockListener, &remoteAddress, &msj, sizeof(message_t), logger, "Planificador");

		// Si hay personajes que mandan mensajes entra al if
		if (iSocketConexion != -1) {

			bool encontrado;

			pthread_mutex_lock(&semNivel);
			//Esto lo pongo pero esta al pedo porque siempre va a recibir del personaje
			switch (msj.type) {

			case PERSONAJE:
				switch (msj.detail) {

				case SALUDO:

					//Me fijo si esta en la cola de finalizados
					if (exist_personaje(nivelPlanif->l_personajesDie, msj.name, &contPj)) {

						//Lo saco de finalizados y lo mando a ready
						pjLevantador = list_remove(nivelPlanif->l_personajesDie, contPj);
						//Actualizo su socket
						pjLevantador->sockID = iSocketConexion;
						//No hace falta pero lo vuelvo a marcar como muerto por las dudas
						pjLevantador->kill   = true;
						pjLevantador->ready  = false;
						list_add(nivelPlanif->l_personajesRdy, pjLevantador);

						// Logueo la modificacion de la cola y notifico por pantalla
						log_trace(logger,"Se agrego el personaje %c a la cola de listos",pjLevantador->name);
						imprimirLista(nivelPlanif->nombre,nivelPlanif->l_personajesRdy,nivelPlanif->l_personajesBlk, proxPj);

					} else {

						// Agrego informacion del personaje
						personajeNuevo.name   = msj.name;
						personajeNuevo.sockID = iSocketConexion;
						personajeNuevo.ready  = false;
						personajeNuevo.kill   = false;

						//cuando un personaje obtenga un recurso hago un list_add_new de ese
						//recurso. Esto ocurrira en el case de TURNO
						personajeNuevo.recursos = list_create();

						//Envio el turno al primer personaje
						if (first_time || (!first_time && list_size(nivelPlanif->l_personajesRdy) == 0)) {
							first_time = false;
							soyElPrimerPersonajeDameUnTurno = 1;
							sockPrimerPersonaje = iSocketConexion;
						}

						//Voy a usar esto para el deadlock
						personajeNuevo.index = contPersonajes++;

						// Agrego el personaje a la cola de pre-listos
						list_add_new(nivelPlanif->l_personajesRdy,(void *) &personajeNuevo, sizeof(personaje_t));

						// Logueo la modificacion de la cola y notifico por pantalla
						log_trace(logger,"Se agrego el personaje %c a la cola de listos",personajeNuevo.name);
						imprimirLista(nivelPlanif->nombre,nivelPlanif->l_personajesRdy,nivelPlanif->l_personajesBlk, proxPj);
					}

					reMultiplexar = false;

					log_trace(logger, "Se conecto personaje %c en socket %d", msj.name,  iSocketConexion);

					// Armo mensaje de SALUDO con el nivel
					msj.type = SALUDO;

					// Envía el "Saludo" al nivel avisando que un personaje quiere jugar
					enviaMensaje(nivelPlanif->sock, &msj, sizeof(message_t), logger, "Saludo Nivel");

					// Recibo el "Saludo" y agrego la informacion al personaje
					recibeMensaje(nivelPlanif->sock, &msj, sizeof(message_t), logger, "Recibe SALUDO del nivel con pos inicial");

					// Agrego la informacion del personaje que me faltaba
					personajeNuevo.posX = msj.detail;
					personajeNuevo.posY = msj.detail2;

					//Armo SALUDO para el planificador
					msj.type    = SALUDO;
					msj.detail  = personajeNuevo.posX;
					msj.detail2 = personajeNuevo.posY;

					// Envio la informacion de posicion al personaje
					enviaMensaje(iSocketConexion, &msj, sizeof(message_t), logger,"Envio SALUDO al personaje");

					break;

				case POSICION_RECURSO:

					// Armo mensaje a enviar al nivel
					msj.type = POSICION_RECURSO;

					// Envio mensaje al nivel con la informacion
					enviaMensaje(nivelPlanif->sock, &msj, sizeof(message_t),logger, "Solicito posicion de recurso al nivel");

					// Recibo informacion de la posicion del recurso
					recibeMensaje(nivelPlanif->sock, &msj, sizeof(message_t),logger, "Recibo posicion de recurso del nivel");

					//Armo mensaje para enviar al personaje
					msj.type = POSICION_RECURSO;
					//msj.detail=pos en X
					//msj.detail2= pos en Y
					//msj.name = simbolo

					// Envio la informacion al personaje
					enviaMensaje(iSocketConexion, &msj, sizeof(msj), logger, "Envio posicion del recurso");

					//Si soy el primer personaje que entra al planificador, entonces le doy un turno
					if (soyElPrimerPersonajeDameUnTurno) {

						for (contPj =0 ; contPj<list_size(nivelPlanif->l_personajesRdy); contPj++) {
							pjLevantador = list_get(nivelPlanif->l_personajesRdy, contPj);

							if (pjLevantador->sockID == sockPrimerPersonaje) {
								break;
							}
						}
						msj.detail = TURNO;
						soyElPrimerPersonajeDameUnTurno = 0;
						usleep(nivelPlanif->retardo);
						log_trace(logger, "%s: Turno(primerPersonaje) para %c con quantum=%d",nivelPlanif->nombre, pjLevantador->name, q);
						enviaMensaje(sockPrimerPersonaje, &msj, sizeof(message_t), logger, "Turno para el primer personaje");

						break;
					}

					for (contPj = 0; contPj<list_size(nivelPlanif->l_personajesRdy); contPj++) {
						pjLevantador = list_get(nivelPlanif->l_personajesRdy, contPj);

						if (pjLevantador->sockID == iSocketConexion) {
							pjLevantador->ready = true;
							break;
						}
					}

					if (pjLevantador->kill) {
						pjLevantador->kill = false;
						reMultiplexar = true;
						sockReMultiplexar = iSocketConexion;
					}

					//Si tengo que volver a multiplexar: esta variable solo la pongo en true en el case de MOVIMIENTO
					if (reMultiplexar) {

						for (contPj = 0; contPj<list_size(nivelPlanif->l_personajesRdy); contPj++) {
							pjLevantador = list_get(nivelPlanif->l_personajesRdy, contPj);

							if (pjLevantador->sockID == sockReMultiplexar) {
								break;
							}
						}

						q++;
						msj.detail = TURNO;
						log_trace(logger, "%s: Turno(reMultiplexar) para %c con quantum=%d",nivelPlanif->nombre, pjLevantador->name, q);

						usleep(nivelPlanif->retardo);
						enviaMensaje(sockReMultiplexar, &msj, sizeof(message_t), logger,"Envia proximo turno");
						break;
					}

					if (dar_turno_al_pj_casi_listo && sock_pj_casi_listo == iSocketConexion) {
						msj.type   = PERSONAJE;
						msj.detail = TURNO;

						log_trace(logger, "%s: Turno(pj_casi_listo) para %c con quantum=%d",nivelPlanif->nombre, pjLevantador->name, q);

						usleep(nivelPlanif->retardo);
						enviaMensaje(iSocketConexion, &msj,sizeof(message_t), logger,"Envia proximo turno");
						dar_turno_al_pj_casi_listo = false;
						break;
					}

					reMultiplexar = false;

					break;
				case TURNO:

					//Si no lo mato un enemigo, le doy turnos normalmente

					//Si hay personajes en la cola de listos: entonces debo darle un turno a alguno de ellos
					if (list_size(nivelPlanif->l_personajesRdy) != 0) {
						//Para darles un turno debo actualizar su quantum

						//--Si ya terminó su quantum debo pasar al proximo personaje
						if (q >= Quantum) {
							//Avanzo al proximo personaje
							proxPj++;

							if (proxPj== list_size(nivelPlanif->l_personajesRdy))
								proxPj = 0;

							q = 1;
						} else {
							q++;
						}
						//Uso la variable proxPj para sacar el personaje al que debo darle el turno

						pjLevantador = (personaje_t *) list_get_data(nivelPlanif->l_personajesRdy, proxPj);

						//Esta parte es para cuando quiera mandar un turno nates de que haya recibido la posicion
						//Esto valida si hay un personaje que todavia no haya recibido la pos de su recuros y que no le mande un turno ahi
						//Ademas va a ser siempre un personaje nuevo a ejecutar y por lo tanto con q=1
						if ((list_size(nivelPlanif->l_personajesRdy)) >1 && (q==1) && !pjLevantador->ready) {
							dar_turno_al_pj_casi_listo = true;
							sock_pj_casi_listo = pjLevantador->sockID;
							break;
						}

						msj.type   = PERSONAJE;
						msj.detail = TURNO;

						log_trace(logger, "%s: Turno(TURNO) para %c con quantum=%d",nivelPlanif->nombre, pjLevantador->name, q);

						usleep(nivelPlanif->retardo);
						enviaMensaje(pjLevantador->sockID, &msj,sizeof(message_t), logger,"Envia proximo turno");

					} else {
						log_error(logger, "Me quedo en cero: %d",list_size(nivelPlanif->l_personajesRdy));
					}

					reMultiplexar = false;
					break;

				case MOVIMIENTO:

					// Cicla los personajes y busca uno a partir de su socket
					for (contPj = 0;contPj < list_size(nivelPlanif->l_personajesRdy);contPj++) {
						pjLevantador = (personaje_t*) list_get(nivelPlanif->l_personajesRdy, contPj);

						if (pjLevantador->sockID == iSocketConexion) {
							break;
						}

					}

					msj.type = MOVIMIENTO;
					msj.detail = msj.name;
					//msjPlan.detail2 = calculaMovimiento
					msj.name = pjLevantador->name;

					// Le envio al nivel el movimiento del personaje
					enviaMensaje(nivelPlanif->sock, &msj, sizeof(message_t), logger, "Envio movimiento del personaje");

					recibeMensaje(nivelPlanif->sock, &msj, sizeof(message_t), logger, "Recibo estado en el que quedo el personaje");

					if (msj.detail == MUERTO_ENEMIGOS) {

						bool _search_by_sockID(personaje_t *pjAux) {
							return (pjAux->sockID == iSocketConexion);
						}

						encontrado = buscarPersonajePor(nivelPlanif->l_personajesRdy, nivelPlanif->l_personajesBlk,
														iSocketConexion,(void *)_search_by_sockID, pjLevantador);

						desbloquearPersonajes(nivelPlanif->l_personajesBlk, nivelPlanif->l_personajesRdy,
												pjLevantador, encontrado, nivelPlanif->nombre, proxPj);

						msj.type = SALIR;
						msj.name = pjLevantador->name;

						enviaMensaje(nivelPlanif->sock, &msj, sizeof(msj), logger,"Salida del nivel");

						recibeMensaje(nivelPlanif->sock, &msj, sizeof(msj), logger,"Recibe confirmacion del nivel");

						//Limpio la lista de recursos que tenia hasta que se murio
						list_clean(pjLevantador->recursos);
						//Lo marco como muerto y lo desmarco cuando pide POSICION_RECURSO
						pjLevantador->kill = true;
						//Lo agrego a la cola de finalizados y lo saco cuando manda msj en el case SALUDO
						list_add(nivelPlanif->l_personajesDie, pjLevantador);

						msj.type=MOVIMIENTO;
						msj.detail = MUERTO_ENEMIGOS;
						enviaMensaje(iSocketConexion, &msj, sizeof(msj), logger, "El personaje ha perdido una vida por enemigos");

						if (list_size(nivelPlanif->l_personajesRdy) > 0) {
							proxPj--;

							if (list_size(nivelPlanif->l_personajesRdy) == proxPj || proxPj<0) {
								proxPj = 0;
							}

							q = 1;
						}

						imprimirLista(nivelPlanif->nombre, nivelPlanif->l_personajesRdy, nivelPlanif->l_personajesBlk, proxPj);

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
							list_add(nivelPlanif->l_personajesBlk,(personaje_t*) list_remove(nivelPlanif->l_personajesRdy,contPj));

							//Acabo de sacar a un personaje en la cola de listos, entonces el proxPj "apunta" a un lugar donde ya no esta ese personaje
							//Entonces lo resto
							proxPj--;

							imprimirLista(nivelPlanif->nombre, nivelPlanif->l_personajesRdy, nivelPlanif->l_personajesBlk, (contPj - 1));

							//Si bien quedo bloqueado, igual tengo que aumentarle su quantum
							q = Quantum + 1;

						} else {//Si se le otorgo el recurso

							log_info(logger, "Personaje: %c se le otorgo: %c",pjLevantador->name, msj.detail2);

							//Pongo esto para volver a multiplexar porque sino no da mas turnos: tal vez haya que sacar esto mas adelante. TODO
							if (list_size(nivelPlanif->l_personajesRdy) > 0) {
								reMultiplexar = true;
								sockReMultiplexar = iSocketConexion;
								pjLevantador->ready = false;
								dar_turno_al_pj_casi_listo = false;
							}
						}

					} else { //Si no habia llegado al recurso y solo se habia movido
						log_info(logger, "Personaje: %c se movio",pjLevantador->name);
						pjLevantador->ready = true;
					}

					if (msj.detail!=BLOCK) {
						msj.type = MOVIMIENTO;
						msj.detail=NADA;
						enviaMensaje(iSocketConexion, &msj, sizeof(msj), logger, "El personaje se movio");
					}


					break;

				case SALIR: //En el case SALIR principalmente Libero memoria y recursos en el nivel

					/*
					 * Si el personaje termino por:
					 *
					 * porque finalizo su plan de niveles: entonces msj.detail2!=MUERTO_ENEMIGOS =>libero recursos
					 *
					 * porque se quedo sin vidas(asesinado por enemigos): entonces msj.detail2=MUERTO_ENEMIGOS => ya libere recursos, debo sacarlo de la cola End y liberar memoria
					 *
					 */

					if (msj.detail2 != MUERTO_ENEMIGOS) {
						//El personaje se quedo sin vidas pero ya devolvio recursos en el case de MOVIMIENTO
						//por lo tanto tiene que liberar memoria solamente y volver a multiplexar

						encontrado = false;

						bool _search_by_sockID(personaje_t *pjAux) {
							return (pjAux->sockID == iSocketConexion);
						}

						encontrado = buscarPersonajePor(nivelPlanif->l_personajesRdy, nivelPlanif->l_personajesBlk,
														iSocketConexion,(void *)_search_by_sockID, pjLevantador);

						desbloquearPersonajes(nivelPlanif->l_personajesBlk, nivelPlanif->l_personajesRdy,
												pjLevantador, encontrado, nivelPlanif->nombre, proxPj);

						msj.type = SALIR;
						msj.name = pjLevantador->name;

						enviaMensaje(nivelPlanif->sock, &msj, sizeof(msj), logger,"Salida del nivel");

						recibeMensaje(nivelPlanif->sock, &msj, sizeof(msj), logger,"Recibe confirmacion del nivel");

						//Limpio la lista de recursos del personaje
						list_clean(pjLevantador->recursos);

						encontrado = true; //La pongo en true para marcar que saco un personaje de ready: modifico proxPj

					} else {

						for (contPj=0; contPj < list_size(nivelPlanif->l_personajesDie); contPj++) {
							pjLevantador = list_get(nivelPlanif->l_personajesDie, contPj);

							if (pjLevantador->sockID == iSocketConexion) {
								list_remove(nivelPlanif->l_personajesDie, contPj);
								break;
							}
						}
						encontrado = false; //Saco un personaje de die, no de ready: no modifico proxPj
					}

					msj.type = SALIR;
					msj.detail = EXIT_SUCCESS;
					char *msjInfo = malloc(sizeof(char) * 100);
					sprintf(msjInfo, "Confirmacion de Salir al personaje %c", pjLevantador->name);
					enviaMensaje(iSocketConexion, &msj, sizeof(msj), logger, msjInfo);

					//Libero memoria
					free(msjInfo);
					free(pjLevantador->recursos);
					free(pjLevantador);

					//Forzamos otro mensaje de turno para volver a multiplexar
					if (list_size(nivelPlanif->l_personajesRdy) > 0) {

						proxPj--;

						if (list_size(nivelPlanif->l_personajesRdy) == proxPj || proxPj<0) {
							proxPj = 0;
						}

						q = 1;

						personaje_t *pjAux = (personaje_t *) list_get(nivelPlanif->l_personajesRdy, proxPj);

						if (pjAux == NULL) {
							log_debug(logger, "hace mal el gett %d", proxPj);
							exit(EXIT_FAILURE);
						} else
							log_debug(logger, "hace bien el get%d", proxPj);

						imprimirLista(nivelPlanif->nombre, nivelPlanif->l_personajesRdy,
								nivelPlanif->l_personajesBlk, proxPj);

						log_trace(logger, "%s: Turno(salir) para %c con quantum=%d",nivelPlanif->nombre, pjAux->name, q);

						//--Cachear pj
						msj.detail = TURNO;
						usleep(nivelPlanif->retardo);
						enviaMensaje(pjAux->sockID, &msj, sizeof(message_t), logger, "Forzado de turno");//TODO aqui tiene un bug

					}
					log_debug(logger, "Libere recursos");
					break; //Fin del case SALIR

				} //Fin de switch(msj.detail)

				break; //Fin del case PERSONAJE

			case NIVEL: //Este case solo se ejecuta la primera vez que envia un mensaje el nivel cuando apenas se conecta al planificador
				switch (msj.detail) {
				case WHATS_UP:
					nivelPlanif->sock = iSocketConexion;
					log_debug(logger, "El nivel %s esta conectado en socket:%d",nivelPlanif->nombre, nivelPlanif->sock);
					break;
				}

				break;

			} //Fin de switch(msj.type)

			pthread_mutex_unlock(&semNivel);

		} //Fin de if(i!=-1)

	} //Fin de while(1)

	return NULL ;
}

int encontreNivel(nivel_t *nivelAux, char *nameToSearch) {
	return ((nivelAux != NULL )&& string_equals_ignore_case(nivelAux->nombre, nameToSearch));
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
					enviaMensaje(pjLevantadorBlk->sockID, &msj,sizeof(msj), logger,"Se desbloqueo un personaje");

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

bool buscarPersonajePor(t_list *ready, t_list *block, int i, bool(*condition)(void*), personaje_t *pjLevantador) {

	int contPj;
	bool encontrado = false;

	//Lo busco en la lista de bloqueados y lo saco de la misma en caso de que este
	for (contPj = 0;contPj < list_size(block);contPj++) {
		//Levanto un personaje
		pjLevantador = list_get(block,contPj);
		//Lo comparo segun su socket y segun el socket que envio este mensaje
		if (condition(pjLevantador)) {
			encontrado = true; //Si lo encontras en Blk, cambiamos el flag
			list_remove(block, contPj); // Y sacamos al personaje de la lista de bloqueados
			break;
		}
	}

	//Si no lo encuentra: lo busco en la lista de Ready y lo saco de la misma
	if (!encontrado) { //Si no lo encontras, buscarlo en rdy
		for (contPj = 0;contPj < list_size(ready);contPj++) { //Cicla los personajes
			pjLevantador = list_get(ready, contPj);
			if (condition(pjLevantador)) { // Si lo encuentra en lista de ready
				list_remove(ready, contPj); // Lo sacamos de la lista
				break;
			}
		}
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
