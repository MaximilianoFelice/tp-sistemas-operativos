/*
 * Sistemas Operativos - Super Mario Proc RELOADED.
 * Grupo       : C o no ser.
 * Nombre      : personaje.c.
 * Descripcion : Este archivo contiene la implementacion de las
 * funciones usadas por el personaje.
 */

#include "personaje.h"

pthread_mutex_t semPersonaje;

char simbolo;
int vidas;
t_config *configPersonaje;
t_list *listaNiveles;
t_log *logger;
char * ip_plataforma;
char * puerto_orq;
char * nombre_pj;
int r = 0;
bool muertePorSenial=false; //Cuando se activa, se cierra t0d0, no importan las vidas
bool inicializeVidas = false;

int main(int argc, char*argv[]) {

	pthread_mutex_init(&semPersonaje, NULL ); // TODO Usar para algo.

	signal(SIGTERM, restarVidas);
	signal(SIGUSR1, aumentarVidas);
	signal(SIGINT, morirSenial);

	// TODO manejar las señales...
	// Inicializa el log.
	logger = logInit(argv, "PERSONAJE");

	// TODO CREAR FUNCION QUE CARGUE LAS VARIABLES DE CONFIG
	// ======================
	// Creamos el archivo de Configuración
	// FIXME Implementar validacion de la existencia de las variables globales
	configPersonaje = config_try_create(argv[1], "nombre,simbolo,planDeNiveles,vidas,orquestador");

	// Obtenemos el nombre - global de solo lectura
	nombre_pj = config_get_string_value(configPersonaje, "nombre");

	// Obtenemos el simbolo - global de solo lectura
	simbolo = config_get_string_value(configPersonaje, "simbolo")[0];

	// Obetenemos los datos del orquestador
	char * dir_orq = config_get_string_value(configPersonaje, "orquestador");
	//todo implementa funcion que divida el ip del puerto
	ip_plataforma  = strtok(dir_orq, ":"); // Separar ip - Global
	puerto_orq 	   = strtok(NULL, ":"); // Separar puerto - Local

	char** niveles = config_try_get_array_value(configPersonaje, "planDeNiveles");
	t_list* listaObjetivos;
	listaNiveles = list_create();
	int j, i = 0;
	nivel_t aux;
	// ======================

	char *stringABuscar = malloc(sizeof(char) * 25);

	int cantThreads = 0;

	//Cuento la cantidad de threads que voy a tirar: uno por cada nivel
	while (niveles[cantThreads] != NULL ) {
		cantThreads++;
	}

	threadNivel_t *hilosNiv;
	hilosNiv = calloc(cantThreads, sizeof(threadNivel_t));

	//Armamos lista de niveles con sus listas de objetivos del config
	while (niveles[i] != NULL ) {  //Cicla los niveles

		sprintf(stringABuscar, "obj[%s]", niveles[i]); //Arma el string a buscar

		//TODO pasarlo a la funcion que cargue las variables globales del archivo de configuracion
		// =====================
		char** objetivos = config_try_get_array_value(configPersonaje,
				stringABuscar);        //Lo busco

		j = 0;

		//Por cada uno, genero una lista
		listaObjetivos = list_create();
		while (objetivos[j] != NULL ) { //Vuelvo a ciclar por objetivos
			list_add_new(listaObjetivos, objetivos[j], sizeof(char)); //Armo la lista
			j++;
		}

		aux.nomNivel = malloc(sizeof(char) * strlen(niveles[i]));
		strcpy(aux.nomNivel, niveles[i]);
		aux.Objetivos = listaObjetivos;
		list_add_new(listaNiveles, &aux, sizeof(nivel_t));

		//creo estructura del nivel que va a jugar cada hilo
		hilosNiv[i].nivel.nomNivel = aux.nomNivel;
		hilosNiv[i].nivel.Objetivos = aux.Objetivos;
		hilosNiv[i].nivel.num_of_thread = i;

		// =====================


		//Tiro el hilo para jugar de cada nivel
		if (pthread_create(&hilosNiv[i].thread, NULL, jugar, (void *) &hilosNiv[i].nivel)) {
			log_error(logger, "pthread_create: %s", strerror(errno));
			exit(EXIT_FAILURE);
		}

		i++;
	}
	free(stringABuscar);

	char *join_return;
	for (i = 0; i < cantThreads; i++) {
		pthread_join(hilosNiv[i].thread, (void**)&join_return);
	}

	//TODO msj al orquestador FIN_PLAN_NIVELES.
	//Cuando terminaron todos los niveles. Manda msj al orquestador de que ya termino todos sus niveles

	if(join_return != NULL)
		log_debug(logger, "El personaje %c %s", simbolo, join_return);

	log_destroy(logger);
	config_destroy(configPersonaje); // TODO PASAR A IMPLEMENTACION GLOBAL
	for (j = 0; j < list_size(listaNiveles); j++) {
		nivel_t *aux = (nivel_t *) list_get(listaNiveles, j);
		list_destroy(aux->Objetivos);
	}
	list_destroy_and_destroy_elements(listaNiveles, (void *) nivel_destroyer);

	pthread_mutex_destroy(&semPersonaje);

	exit(EXIT_SUCCESS);

}

static void nivel_destroyer(nivel_t*nivel) {
	free(nivel->nomNivel);
	free(nivel);
}

void *jugar(void *args) {

	//Recupero la informacion del nivel en el que juega el personaje
	nivel_t *nivelQueJuego;
	nivelQueJuego = (nivel_t *) args;

	int currObj; //objetivo actual
	int posX = 0, posY = 0;
	int posRecursoX, posRecursoY;

	//Variables para conectarme al orquestador y planificador
	int sockOrq, sockPlan;

	//fixme el socket orquestador deberia ser global y llamarse socketPlataforma,  el socket del planificador no deberia existir

	char * ip_planif = malloc(sizeof(char) * 23);  //TODO hacer free
	int puertoPlanif;
	bool finalice = false;
	bool murioPersonaje = false;

	while (1) {//TODO BORRAR este while, no se usa


		// TODO Armar una funcion que genere las variables globales con el archivo de config.
		// ============================== PASAR A VARIABLE GLOBAL
		if (!inicializeVidas) { //Si no inicialice vidas, las inicializo
			vidas = config_get_int_value(configPersonaje, "vidas"); //Obtenemos las vidas
			inicializeVidas = true; //Esta variable solo se pone en false cuando el chaboncito se queda sin vidas y quiere reiniciar
			//reiniciar=false;
		}
		// ==============================

		//Setea los flags del inicio
		finalice = false;
		murioPersonaje = false;

		while (vidas > 0) {

			murioPersonaje = false;
			finalice = false;

			log_info(logger, "Vidas de %c: %d", simbolo, vidas);
			message_t msjPlan;

			// FIXME La conexion se debe realizar fuera del while, ya que la unica posibilidad de que
			// se reinicie la conexion es que el personaje se quede sin vidas y desee reiniciar.
			// Hay que chequear, para ello, que al morir el personaje realize las acciones necesarias.
			sockOrq = connectServer(ip_plataforma, atoi(puerto_orq), logger, "orquestador");

			handshake_orq(&sockOrq, &puertoPlanif, ip_planif, nivelQueJuego->nomNivel);

			//todo sacar el coonect server y usar directamente el sockOrq
			// =======================
			sockPlan = connectServer(ip_plataforma, puertoPlanif, logger, "planificador");

			handshake_planif(&sockPlan, &posX, &posY);
			// =======================

			// Por cada objetivo del nivel,
			for (currObj=0; currObj < list_size(nivelQueJuego->Objetivos); currObj++) {

				murioPersonaje = false;

				//agarra un recurso de la lista de objetivos del nivel
				char* recurso = (char*) list_get_data(nivelQueJuego->Objetivos,	currObj);

				//TODO pasar a una funcion externa el pedido
				// =======================
				//pide la posicion del recurso
				msjPlan.type = PERSONAJE;
				msjPlan.detail = POSICION_RECURSO;
				msjPlan.detail2 = *recurso;
				msjPlan.name = simbolo;

				enviaMensaje(sockPlan, &msjPlan, sizeof(message_t), logger,"Solicitud de Posicion del recurso");

				recibeMensaje(sockPlan, &msjPlan, sizeof(message_t), logger,"Recibo posicion de recurso");


				//valida que lo que recibe sea una posicion

				if (msjPlan.type == POSICION_RECURSO) {
					posRecursoX = msjPlan.detail;
					posRecursoY = msjPlan.detail2;
				} else {
					log_error(logger,"Llegaron (%d, %d, %c, %d) cuando debía llegar POSICION_RECURSO", msjPlan.type, msjPlan.detail, msjPlan.detail2,msjPlan.name);
					exit(EXIT_FAILURE);
				}


				// =======================


				while (1) {

					//FIXME modificar la funcion de muerte por senal y que lo mate y llame a una funcion de muerte que pare to do y pregunte si quiere volver a jugar
					if (validarSenial(&murioPersonaje) || vidas<=0)
						break;

					//Espera que el planificador le de el turno
					recibeMensaje(sockPlan, &msjPlan, sizeof(message_t), logger, "Espero turno");

					//fixme ver si son bloqueantes y si los necesito
					if (validarSenial(&murioPersonaje) || vidas<=0)
						break;

					//TODO transformarlo en una funcion validar Mensaje del Turno()
					// =======================
					//Valido que el mensaje de turno sea correcto
					if (msjPlan.detail != TURNO) {
						log_error(logger, "Llegaron (detail: %d, detail2: %d, name: %d, type: %d) cuando debía llegar TURNO",
								msjPlan.detail, msjPlan.detail2, msjPlan.name,msjPlan.type);
						exit(EXIT_FAILURE);
					}

					log_info(logger, "Habemus turno");

					// =======================

					//TODO refactorizar a una funcion  de mover al personaje
					// =======================
					int mov;

						//TODO refactorizar a una funcion de envio me mensaje de movimiento
						// =======================
						//TODO generar funcion de envio de mensaje al planificador
						// Armamos mensaje de movimiento a realizar con el recurso que necesitamos.
						msjPlan.type = PERSONAJE;
						msjPlan.detail = MOVIMIENTO;
						msjPlan.detail2 = mov = calculaMovimiento(posX, posY,posRecursoX, posRecursoY);
						msjPlan.name = *recurso;

						//Aviso a planificador que me movi
						enviaMensaje(sockPlan, &msjPlan, sizeof(msjPlan), logger, "Requerimiento de Movimiento");

						recibeMensaje(sockPlan, &msjPlan, sizeof(msjPlan), logger, "Confirmación de movimiento");

						// =======================

					//TODO VERIFICAR si chequea aca si esta muerto por un enemigo y que pasa cuando sale del while y entra en el for
					if(estaMuerto(msjPlan.detail, &murioPersonaje))
						break;

					//Actualizo mi posición y de acuerdo a eso armo mensaje de TURNO
					actualizaPosicion(mov, &posX, &posY);

					if (msjPlan.type != MOVIMIENTO) {
						log_error(logger, "Llegaron (detail: %d, detail2:%d, name:%c, type:%d) cuando debía llegar MOVIMIENTO",
								  msjPlan.detail, msjPlan.detail2, msjPlan.name, msjPlan.type);
						exit(EXIT_FAILURE);
					}

					//Si llego al recurso no pide otro turno, solo sale del while a buscar el siguiente recurso, si puede.
					if (posY == posRecursoY && posX == posRecursoX)
						break;
					//TODO: si esta bloqueado por un recurso, el planificador no le confirma el turno. REVISAR TODO
					else { //Si no llego al recurso sigue moviéndose

						//No llegué al recurso
						msjPlan.type = PERSONAJE;
						msjPlan.detail = TURNO;

						enviaMensaje(sockPlan, &msjPlan, sizeof(msjPlan), logger, "Fin de turno");
					}

				} //Fin de while(1) de busqueda de un recurso

				if (murioPersonaje || muertePorSenial || vidas<=0) //sale del for
					break;

			} //Fin de for de objetivos

			//FIXME no deberia cerrar todas las conexiones hasta que se decide que no quiere reiniciar el juego
			//=======================
			if (!murioPersonaje) {
				finalice = true;
				devolverRecursos(&sockPlan, &msjPlan);
				cerrarConexiones(&sockPlan, &sockOrq);
			}

			if(vidas <= 0){
				devolverRecursos(&sockPlan, &msjPlan);
				cerrarConexiones(&sockPlan, &sockOrq);
			}

			//FIXME DEBERIA restarle una vida sola en lugar de matarlo completamente
			if(muertePorSenial){
				devolverRecursos(&sockPlan, &msjPlan);
				cerrarConexiones(&sockPlan, &sockOrq);
			}
			//=======================

			if(murioPersonaje){
				if (vidas<=0) { //Si me quede sin vidas armo un mensaje especial para que el planificador libere memoria
					//TODO preguntar si tiene q reiniciar o no - contemplar estas posibilidades en la funcion de muerte
					//TODO si dice q no = > finalice=true
					armarMsj(&msjPlan, simbolo, PERSONAJE, SALIR, MUERTO_ENEMIGOS);
					enviaMensaje(sockPlan, &msjPlan, sizeof(msjPlan), logger, "Salida al planificador");
					recibeMensaje(sockPlan, &msjPlan, sizeof(msjPlan), logger, "Recibo confirmacion del planificador");
				}
				vidas--;
				log_debug(logger, "Me han matado :/");
				cerrarConexiones(&sockPlan, &sockOrq);
			}

			if(muertePorSenial || finalice)
				break;

		} //Fin del while(vidas>0)

		//TODO borrar
		if (finalice || muertePorSenial || vidas<=0)
			break;

	} //Fin de while(1) de control de reinicio del personaje TODO Borrar

	//Aqui siempre va a terminar porque: termino su nivel bien; se cerro el proceso por señal; se acabaron sus vidas y no quiere reiniciar

	if (vidas<=0) {
		char *exit_return;
		exit_return = strdup("se ha quedado sin vidas y murio :'(");
		pthread_exit((void *)exit_return);
	}
	if (murioPersonaje && muertePorSenial) {// TODO Verificar si se debe evaluar murioPersonaje, porque muertePorSenial ya es condicion suficiente
		char *exit_return;
		exit_return = strdup("ha terminado por senial SIGINT");
		pthread_exit((void *)exit_return);
	}

	char * exit_return = strdup("ha finalizado su plan de niveles correctamente");
	pthread_exit((void *)exit_return);


}

bool estaMuerto(int8_t detail, bool *murioPj){//FIXME no deberia devolver nada porque ya pisa el valor y se usa una sola vez
	if(detail == MUERTO_DEADLOCK)
		return (*murioPj = true);
	if(detail == MUERTO_ENEMIGOS)
		return (*murioPj = true);
	return (*murioPj =false);
}

void handshake_planif(int *sockPlan, int *posX, int *posY) {
	message_t msjPlan;
	msjPlan.type = PERSONAJE;
	msjPlan.detail = SALUDO;
	msjPlan.name = simbolo;

	enviaMensaje(*sockPlan, &msjPlan, sizeof(message_t), logger, "Envia SALUDO al planificador");

	recibeMensaje(*sockPlan, &msjPlan, sizeof(message_t), logger, "Recibo SALUDO del planificador");

	if (msjPlan.type == SALUDO) {
		*posX = msjPlan.detail;
		*posY = msjPlan.detail2;
	} else {
		log_error(logger, "Tipo de msj incorrecto: se esperaba SALUDO");
		exit(EXIT_FAILURE);
	}
}

void handshake_orq(int *sockOrq, int *puertoPlanif, char*ip_planif, char *nom_nivel){
	orq_t msjOrq;
	msjOrq.type = PERSONAJE;
	msjOrq.detail = SALUDO;
	msjOrq.ip[0] = simbolo;
	strcpy(msjOrq.name, nom_nivel);

	enviaMensaje(*sockOrq, &msjOrq, sizeof(orq_t), logger, "Envio de SALUDO al orquestador");

	recibeMensaje(*sockOrq, &msjOrq, sizeof(orq_t), logger, "Recibo puerto e ip de mi planificador en SALUDO");

	if (msjOrq.detail == SALUDO) {
		if (string_equals_ignore_case(msjOrq.ip, ip_plataforma)) {
			strcpy(ip_planif, msjOrq.ip);
			*puertoPlanif = msjOrq.port;
		} else {
			log_warning(logger, "Las ip's del planificador no coinciden: laMia=%s y laQueMePasaron=%s", ip_plataforma, msjOrq.ip);
			exit(EXIT_FAILURE);
		}
	} else {
		if (msjOrq.detail == NADA) {
			log_error(logger, "El nivel solicitado no esta disponible");
			pthread_exit(NULL );
		} else {
			log_error(logger, "Tipo de mensaje incorrecto se esperaba NADA");
			exit(EXIT_FAILURE);
		}
	}
}

void cerrarConexiones(int * sockPlan, int *sockOrq){
	close(*sockPlan);
	close(*sockOrq);
	log_debug(logger, "Cierro conexion con el orquestador y planificador");
}

void morir(char* causaMuerte, int *currObj) {
	log_info(logger, "%s murio por: %s", nombre_pj, causaMuerte);
	*currObj=1000;
	vidas--;
}


bool devolverRecursos(int *sockPlan, message_t *message) {

	message->type = PERSONAJE;
	message->detail = SALIR;
	message->detail2 = NADA;
	message->name = simbolo;

	enviaMensaje(*sockPlan, message, sizeof(message_t), logger, "Salida al planificador");

	recibeMensaje(*sockPlan, message, sizeof(message_t), logger, "Confirmo salida");

	if (message->type == SALIR) {
		log_trace(logger, "Recursos liberados");
		return true;
	} else {
		log_error(logger, "Tipo de msj incorrecto se esperaba SALIR y me llego (type=%d, detail=%d)", message->type, message->detail);
		exit(EXIT_FAILURE);
	}
	return false;
}

//Seniales
bool validarSenial(bool *murioPersonaje){
	if(muertePorSenial){
		return (*murioPersonaje = true);
	}
	else
		return (*murioPersonaje = false);
}

void morirSenial() {
	muertePorSenial=true;
}

void aumentarVidas() {
	vidas++;
	log_info(logger, "Vidas de %c: %d", simbolo, vidas);
}

void restarVidas() {
	vidas--;
	log_info(logger, "Vidas de %c: %d", simbolo, vidas);
}
//Seniales

int calculaMovimiento(int posX, int posY, int posRX, int posRY) {

	if (posX == posRX && posY == posRY)
		return -1;

	while (1) {
		r = rand() % 2;
		if (r) { //Sobre el eje x
			if (r && posX < posRX)
				return DERECHA;
			else if (posX > posRX)
				return IZQUIERDA;
		} else { // Sobre el eje y
			if (posY < posRY)
				return ABAJO;
			else if (posY > posRY)
				return ARRIBA;
		}
	}

	return -700;
}

// Actualiza las variables posicion del personaje a partir del movimiento que recibe por parametro.
void actualizaPosicion(int movimiento, int *posX, int *posY) {
	switch (movimiento) {
// El eje Y es alreves, por eso para ir para arriba hay que restar en el eje y.
	case ARRIBA:
		(*posY)--;
		break;
	case ABAJO:
		(*posY)++;
		break;
	case DERECHA:
		(*posX)++;
		break;
	case IZQUIERDA:
		(*posX)--;
		break;
	}
}
/*
 *
  COSAS PARA AGREGAR

1- Verificar cuando el personaje termina sus objetivos y cuando termine, el envio del mensaje al planificador (tiene que mandar un type = PERSONAJE detail=SALIR + detail2=NADA)
2- Controlar la modificacion de vidas con semaforos
3- Si el personaje pierde todas las vidas debe:
 	*lockear al resto de los threads y preguntar si quiere reiniciar o no.
	*Los hilos bloqueados deberian mandar un mensaje de SALIR + hilos sin vidas y lockearse esperando ser desbloqueados por el hilo que realizo la consulta.
Si no se quiere reiniciar, debe mandar SALIR + MURIO_ENEMIGOS
 * */

