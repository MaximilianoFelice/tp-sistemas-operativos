/*
 * Sistemas Operativos - Super Mario Proc RELOADED.
 * Grupo       : C o no ser.
 * Nombre      : personaje.c.
 * Descripcion : Este archivo contiene la implementacion de las
 * funciones usadas por el personaje.
 */

#include "personaje.h"

pthread_mutex_t semPersonaje;

personajeGlobal_t personaje;
t_config *configPersonaje;
t_log *logger;
char * ip_plataforma;
char * puerto_orq;

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


	// Creamos el archivo de Configuración
	cargarArchivoConfiguracion(argv[1]);
	int cantidadNiveles =list_size(personaje.listaNiveles);
	threadNivel_t *hilosNiv;
	hilosNiv = calloc(cantidadNiveles, sizeof(threadNivel_t));

	int i=0;
	int indiceNivel;
	for( indiceNivel = 0;  indiceNivel < cantidadNiveles; indiceNivel ++) {
		//creo estructura del nivel que va a jugar cada hilo
		hilosNiv[i].nivel.nomNivel = ((nivel_t*) list_get_data(personaje.listaNiveles,indiceNivel))->nomNivel;
		hilosNiv[i].nivel.Objetivos = ((nivel_t*) list_get_data(personaje.listaNiveles,	indiceNivel))->Objetivos;
		hilosNiv[i].nivel.num_of_thread = indiceNivel;

		//Tiro el hilo para jugar de cada nivel
		if (pthread_create(&hilosNiv[i].thread, NULL, jugar, (void *) &hilosNiv[i].nivel)) {
			log_error(logger, "pthread_create: %s", strerror(errno));
			exit(EXIT_FAILURE);
		}

		i++;
	}

	char *join_return;
	for (i = 0; i < cantidadNiveles; i++) {
		pthread_join(hilosNiv[i].thread, (void**)&join_return);
	}

	//TODO msj al orquestador FIN_PLAN_NIVELES.
	//Cuando terminaron todos los niveles. Manda msj al orquestador de que ya termino todos sus niveles

	if(join_return != NULL)
		log_debug(logger, "El personaje %c %s", personaje.simbolo, join_return);

	log_destroy(logger);
	destruirArchivoConfiguracion(configPersonaje);
	for (i = 0; i < list_size(personaje.listaNiveles); i++) {
		nivel_t *aux = (nivel_t *) list_get(personaje.listaNiveles, i);
		list_destroy(aux->Objetivos);
	}
	list_destroy_and_destroy_elements(personaje.listaNiveles, (void *) nivel_destroyer);

	pthread_mutex_destroy(&semPersonaje);

	exit(EXIT_SUCCESS);

}
void destruirArchivoConfiguracion(t_config *configPersonaje){
	config_destroy(configPersonaje);
}

void cargarArchivoConfiguracion(char* archivoConfiguracion){
	//valida que los campos basicos esten en el archivo
	configPersonaje = config_try_create(archivoConfiguracion, "nombre,simbolo,planDeNiveles,vidas,orquestador");

	// Obtenemos el nombre del personaje - global de solo lectura
	personaje.nombre = config_get_string_value(configPersonaje, "nombre");

	// Obtenemos el simbolo - global de solo lectura
	personaje.simbolo = config_get_string_value(configPersonaje, "simbolo")[0];

	personaje.vidasMaximas = config_get_int_value(configPersonaje, "vidas"); //Obtenemos las vidas

	// Obetenemos los datos del orquestador
	char * dir_orq = config_get_string_value(configPersonaje, "orquestador");
	obtenerIpYPuerto(dir_orq, ip_plataforma, puerto_orq);

	//Obtenemos el plan de niveles
	char** niveles = config_try_get_array_value(configPersonaje, "planDeNiveles");
	t_list* listaObjetivos;
	personaje.listaNiveles = list_create();
	int j, i = 0;
	nivel_t aux;

	char *stringABuscar = malloc(sizeof(char) * 25);


	//Armamos lista de niveles con sus listas de objetivos del config
	while (niveles[i] != NULL ) {  //Cicla los niveles

		sprintf(stringABuscar, "obj[%s]", niveles[i]); //Arma el string a buscar

		char** objetivos = config_try_get_array_value(configPersonaje, stringABuscar);

		j = 0;

		//Por cada nivel, genero una lista de objetivos
		listaObjetivos = list_create();
		while (objetivos[j] != NULL ) { //Por cada objetivo
			list_add_new(listaObjetivos, objetivos[j], sizeof(char)); //Agrego a la lista
			j++;
		}

		//agregamos el nivel a la lista de niveles
		aux.nomNivel = malloc(sizeof(char) * strlen(niveles[i]));
		strcpy(aux.nomNivel, niveles[i]);
		aux.Objetivos = listaObjetivos;
		list_add_new(personaje.listaNiveles, &aux, sizeof(nivel_t));

		i++;
	}
	free(stringABuscar);

}

void obtenerIpYPuerto(char *dirADividir, char * ip,  char * puerto){
	ip_plataforma  = strtok(dirADividir, ":"); // Separar ip
	puerto_orq 	   = strtok(NULL, ":"); // Separar puerto

}
static void nivel_destroyer(nivel_t*nivel) {
	free(nivel->nomNivel);
	free(nivel);
}

void *jugar(void *args) {

	//Recupero la informacion del nivel en el que juega el personaje



	personajeIndividual_t personajePorNivel;

	personajePorNivel.posX=0;
	personajePorNivel.posY=0;
	personajePorNivel.nivelQueJuego = (nivel_t *) args;
	//Variables para conectarme al orquestador y planificador
	//int sockOrq, sockPlan;
	int socketPlataforma;
	//fixme el socket orquestador deberia ser global y llamarse socketPlataforma,  el socket del planificador no deberia existir

	char * ip_planif = malloc(sizeof(char) * 23);  //TODO hacer free
	int puertoPlanif;
	bool finalice = false;
	bool murioPersonaje = false;

	while (1) {//FIXME CAMBIAR este while, para adaptarlo a la ejecucion de koopa


		if (!inicializeVidas) { //Si no inicialice vidas, las inicializo
			personaje.vidas=personaje.vidasMaximas;
			inicializeVidas = true; //Esta variable solo se pone en false cuando el chaboncito se queda sin vidas y quiere reiniciar
			//reiniciar=false;
		}

		//Setea los flags del inicio
		finalice = false;
		murioPersonaje = false;

		while (personaje.vidas  > 0) {

			murioPersonaje = false;
			finalice = false;

			log_info(logger, "Vidas de %c: %d", personaje.simbolo, personaje.vidas);
		//	message_t msjPlan; //TODO crear el mensaje con la estructura actual


			// todo Hay que chequear que al morir el personaje realize las acciones necesarias.
			personajePorNivel.socketPlataforma= connectServer(ip_plataforma, atoi(puerto_orq), logger, "orquestador");

			//fixme adaptar a los nuevos mensajes
			handshake_orq(&personajePorNivel);

			//fixme adaptar a los nuevos mensajes
			handshake_planif(&personajePorNivel);


			// Por cada objetivo del nivel,
			for (personajePorNivel.objetivoActual=0; personajePorNivel.objetivoActual < list_size(personajePorNivel.nivelQueJuego->Objetivos); personajePorNivel.objetivoActual++) {

				murioPersonaje = false;

				//agarra un recurso de la lista de objetivos del nivel
				char* recurso = (char*) list_get_data(personajePorNivel.nivelQueJuego->Objetivos,	personajePorNivel.objetivoActual);

				pedirPosicionRecurso(&personajePorNivel, recurso);

				while (1) {

					//FIXME modificar la funcion de muerte por senal y que lo mate y llame a una funcion de muerte que pare to do y pregunte si quiere volver a jugar
					if (validarSenial(&murioPersonaje) || personaje.vidas<=0)
						break;

					//Espera que el planificador le de el turno
					recibeMensaje(personajePorNivel.socketPlataforma, &msjPlan, sizeof(message_t), logger, "Espero turno");

					//fixme ver si son bloqueantes y si los necesito
					if (validarSenial(&murioPersonaje) || personaje.vidas<=0)
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
						msjPlan.detail2 = mov = calculaMovimiento(personajePorNivel);
						msjPlan.name = *recurso;

						//Aviso a planificador que me movi
						enviaMensaje(personajePorNivel.socketPlataforma, &msjPlan, sizeof(msjPlan), logger, "Requerimiento de Movimiento");

						recibeMensaje(personajePorNivel.socketPlataforma, &msjPlan, sizeof(msjPlan), logger, "Confirmación de movimiento");

						// =======================

					//TODO VERIFICAR si chequea aca si esta muerto por un enemigo y que pasa cuando sale del while y entra en el for
					if(estaMuerto(msjPlan.detail, &murioPersonaje))
						break;

					//Actualizo mi posición y de acuerdo a eso armo mensaje de TURNO
					actualizaPosicion(mov, &personajePorNivel);

					if (msjPlan.type != MOVIMIENTO) {
						log_error(logger, "Llegaron (detail: %d, detail2:%d, name:%c, type:%d) cuando debía llegar MOVIMIENTO",
								  msjPlan.detail, msjPlan.detail2, msjPlan.name, msjPlan.type);
						exit(EXIT_FAILURE);
					}

					//Si llego al recurso no pide otro turno, solo sale del while a buscar el siguiente recurso, si puede.
					if (personajePorNivel.posY == personajePorNivel.posRecursoY && personajePorNivel.posX == personajePorNivel.posRecursoX)
						break;
					//TODO: si esta bloqueado por un recurso, el planificador no le confirma el turno. REVISAR TODO
					else { //Si no llego al recurso sigue moviéndose

						//No llegué al recurso
						msjPlan.type = PERSONAJE;
						msjPlan.detail = TURNO;

						enviaMensaje(personajePorNivel.socketPlataforma, &msjPlan, sizeof(msjPlan), logger, "Fin de turno");
					}

				} //Fin de while(1) de busqueda de un recurso

				if (murioPersonaje || muertePorSenial || personaje.vidas<=0) //sale del for
					break;

			} //Fin de for de objetivos

			//FIXME no deberia cerrar todas las conexiones hasta que se decide que no quiere reiniciar el juego
			//=======================
			if (!murioPersonaje) {
				finalice = true;
				devolverRecursos(&personajePorNivel.socketPlataforma, &msjPlan);
				cerrarConexiones(&personajePorNivel.socketPlataforma);
			}

			if(personaje.vidas <= 0){
				devolverRecursos(&personajePorNivel.socketPlataforma, &msjPlan);
				cerrarConexiones(&personajePorNivel.socketPlataforma);
			}

			//FIXME DEBERIA restarle una vida sola en lugar de matarlo completamente
			if(muertePorSenial){
				devolverRecursos(&personajePorNivel.socketPlataforma, &msjPlan);
				cerrarConexiones(&personajePorNivel.socketPlataforma);
			}
			//=======================

			if(murioPersonaje){
				if (personaje.vidas<=0) { //Si me quede sin vidas armo un mensaje especial para que el planificador libere memoria
					//TODO preguntar si tiene q reiniciar o no - contemplar estas posibilidades en la funcion de muerte
					//TODO si dice q no = > finalice=true
					armarMsj(&msjPlan, personaje.simbolo, PERSONAJE, SALIR, MUERTO_ENEMIGOS);
					enviaMensaje(personajePorNivel.socketPlataforma, &msjPlan, sizeof(msjPlan), logger, "Salida al planificador");
					recibeMensaje(personajePorNivel.socketPlataforma, &msjPlan, sizeof(msjPlan), logger, "Recibo confirmacion del planificador");
				}
				personaje.vidas--;
				log_debug(logger, "Me han matado :/");
				cerrarConexiones(&personajePorNivel.socketPlataforma);
			}

			if(muertePorSenial || finalice)
				break;

		} //Fin del while(vidas>0)

		//TODO borrar
		if (finalice || muertePorSenial || personaje.vidas<=0)
			break;

	} //Fin de while(1) de control de reinicio del personaje TODO Borrar

	//Aqui siempre va a terminar porque: termino su nivel bien; se cerro el proceso por señal; se acabaron sus vidas y no quiere reiniciar

	if (personaje.vidas<=0) {
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

void pedirPosicionRecurso(personajeIndividual_t* personajePorNivel, char recurso){
	message_t *msjPlan;
	//pide la posicion del recurso
	msjPlan.type = PERSONAJE;
	msjPlan.detail = POSICION_RECURSO;
	msjPlan.detail2 = *recurso;
	msjPlan.name = personaje.simbolo;

	enviaMensaje(personajePorNivel.socketPlataforma, &msjPlan, sizeof(message_t), logger,"Solicitud de Posicion del recurso");

	recibeMensaje(personajePorNivel.socketPlataforma, &msjPlan, sizeof(message_t), logger,"Recibo posicion de recurso");


	//valida que lo que recibe sea una posicion

	if (msjPlan.type == POSICION_RECURSO) {
		personajePorNivel.posRecursoX = msjPlan.detail;
		personajePorNivel.posRecursoY = msjPlan.detail2;
	} else {
		log_error(logger,"Llegaron (%d, %d, %c, %d) cuando debía llegar POSICION_RECURSO", msjPlan.type, msjPlan.detail, msjPlan.detail2,msjPlan.name);
		exit(EXIT_FAILURE);
	}

}

bool estaMuerto(int8_t detail, bool *murioPj){//FIXME no deberia devolver nada porque ya pisa el valor y se usa una sola vez
	if(detail == MUERTO_DEADLOCK)
		return (*murioPj = true);
	if(detail == MUERTO_ENEMIGOS)
		return (*murioPj = true);
	return (*murioPj =false);
}

void handshake_planif(personajeIndividual_t *personajePorNivel) {
	message_t msjPlan;
	msjPlan.type = PERSONAJE;
	msjPlan.detail = SALUDO;
	msjPlan.name = personaje.simbolo;

	enviaMensaje(*personajePorNivel->socketPlataforma, &msjPlan, sizeof(message_t), logger, "Envia SALUDO al planificador");

	recibeMensaje(*personajePorNivel->socketPlataforma, &msjPlan, sizeof(message_t), logger, "Recibo SALUDO del planificador");

	if (msjPlan.type == SALUDO) {
		*personajePorNivel->posX = msjPlan.detail;
		*personajePorNivel->posY = msjPlan.detail2;
	} else {
		log_error(logger, "Tipo de msj incorrecto: se esperaba SALUDO");
		exit(EXIT_FAILURE);
	}
}

void handshake_orq(personajeIndividual_t *personajePorNivel){

	//-fixme comprobar que este bien adaptado

	tPaquete paqueteHandshake;
	paqueteHandshake.type   = P_HANDSHAKE;
	paqueteHandshake.length = 0;
	enviarPaquete(*personajePorNivel->socketPlataforma, &paqueteHandshake, logger, "Handshake del personaje con la plataforma");


//	enviaMensaje(*personajePorNivel->socketPlataforma, &msjOrq, sizeof(orq_t), logger, "Envio de SALUDO al orquestador");

	recibeMensaje(*personajePorNivel->socketPlataforma, &paqueteHandshake, sizeof(orq_t), logger, "Recibo respuesta del handshake con la plataforma");

}

void cerrarConexiones(int * socketPlataforma){
	close(*socketPlataforma);
	log_debug(logger, "Cierro conexion con la plataforma");
}
/*
 *
 * lo comento porque todavia no se uso
 *
 *
 *
void morir(char* causaMuerte, personajeIndividual_t personajePorNivel) {
	log_info(logger, "%s murio por: %s", personaje.nombre, causaMuerte);
	*personajePorNivel.objetivoActual=1000;
	personaje.vidas--;
}*/


bool devolverRecursos(int *socketPlataforma, message_t *message) {

	message->type = PERSONAJE;
	message->detail = SALIR;
	message->detail2 = NADA;
	message->name = personaje.simbolo;

	enviaMensaje(*socketPlataforma, message, sizeof(message_t), logger, "Salida al planificador");

	recibeMensaje(*socketPlataforma, message, sizeof(message_t), logger, "Confirmo salida");

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
	personaje.vidas ++;
	log_info(logger, "Vidas de %c: %d", personaje.simbolo, personaje.vidas);
}

void restarVidas() {
	personaje.vidas--;
	log_info(logger, "Vidas de %c: %d", personaje.simbolo, personaje.vidas);
}
//Seniales

int calculaMovimiento(personajeIndividual_t personajePorNivel){

	if (personajePorNivel.posX == personajePorNivel.posRecursoX && personajePorNivel.posY == personajePorNivel.posRecursoY)
		return -1;

	while (1) {
		r = rand() % 2;
		if (r) { //Sobre el eje x
			if (r && personajePorNivel.posX < personajePorNivel.posRecursoX)
				return DERECHA;
			else if (personajePorNivel.posX > personajePorNivel.posRecursoX)
				return IZQUIERDA;
		} else { // Sobre el eje y
			if (personajePorNivel.posY < personajePorNivel.posRecursoY)
				return ABAJO;
			else if (personajePorNivel.posY > personajePorNivel.posRecursoY)
				return ARRIBA;
		}
	}

	return -700;
}

// Actualiza las variables posicion del personaje a partir del movimiento que recibe por parametro.
void actualizaPosicion(int movimiento, personajeIndividual_t *personajePorNivel) {
	switch (movimiento) {
// El eje Y es alreves, por eso para ir para arriba hay que restar en el eje y.
	case ARRIBA:
		(*personajePorNivel->posY)--;
		break;
	case ABAJO:
		(*personajePorNivel->posY)++;
		break;
	case DERECHA:
		(*personajePorNivel->posX)++;
		break;
	case IZQUIERDA:
		(*personajePorNivel->posY)--;
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

