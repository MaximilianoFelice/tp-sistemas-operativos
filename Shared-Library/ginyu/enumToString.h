///*
// * caca.h
// *
// *  Created on: 01/12/2013
// *      Author: utnso
// */
//
//#ifndef CACA_H_
//#define CACA_H_
//
//#include <stdbool.h>
//#include <stdlib.h>
//#include <stdio.h>
//
///*
// * Defino todos los enum's de esta forma porque asi los mantengo sincronizado con su respectivo string
// *
// * Formato del tipo del paquete:
// * 		[emisor]_[mensaje]
// * Emisor:
// * 		N: Nivel
// * 		P: Personaje
// * 		PL: Plataforma
// *
// * 	aviso: significa que no manda nada
// */
//#define FOREACH_TYPEMSJ(TYPEMSJ) 				\
//		TYPEMSJ(PL_HANDSHAKE) 					\
//		TYPEMSJ(PL_CONEXION_PERS)				\
//		TYPEMSJ(PL_PERSONAJE_REPETIDO)			\
//		TYPEMSJ(PL_POS_RECURSO)					\
//		TYPEMSJ(PL_OTORGA_TURNO)				\
//		TYPEMSJ(PL_CONFIRMACION_MOV)			\
//		TYPEMSJ(PL_MOV_PERSONAJE)				\
//		TYPEMSJ(PL_DESCONECTARSE_MUERTE)		\
//		TYPEMSJ(PL_MUERTO_POR_ENEMIGO)			\
//		TYPEMSJ(PL_MUERTO_POR_DEADLOCK)			\
//		TYPEMSJ(PL_CONFIRMACION_ELIMINACION)	\
//		TYPEMSJ(PL_NIVEL_YA_EXISTENTE)			\
//		TYPEMSJ(PL_NIVEL_INEXISTENTE)			\
//		TYPEMSJ(PL_SOLICITUD_RECURSO)			\
//		TYPEMSJ(PL_RECURSO_INEXISTENTE)			\
//		TYPEMSJ(PL_RECURSO_OTORGADO)			\
//		TYPEMSJ(PL_DESCONEXION_PERSONAJE)		\
//		TYPEMSJ(N_HANDSHAKE)					\
//		TYPEMSJ(N_CONEXION_EXITOSA)				\
//		TYPEMSJ(N_PERSONAJE_YA_EXISTENTE)		\
//		TYPEMSJ(N_CONFIRMACION_ELIMINACION)		\
//		TYPEMSJ(N_MUERTO_POR_ENEMIGO)			\
//		TYPEMSJ(N_PERSONAJES_DEADLOCK)			\
//		TYPEMSJ(N_ESTADO_PERSONAJE)				\
//		TYPEMSJ(N_POS_RECURSO)					\
//		TYPEMSJ(N_DATOS)						\
//		TYPEMSJ(N_ACTUALIZACION_CRITERIOS)		\
//		TYPEMSJ(N_ENTREGA_RECURSO)				\
//		TYPEMSJ(N_CONFIRMACION_MOV)				\
//		TYPEMSJ(N_PERSONAJE_INEXISTENTE)		\
//		TYPEMSJ(N_RECURSO_INEXISTENTE)			\
//		TYPEMSJ(P_HANDSHAKE)					\
//		TYPEMSJ(P_MOVIMIENTO)					\
//		TYPEMSJ(P_POS_RECURSO)					\
//		TYPEMSJ(P_SIN_VIDAS)					\
//		TYPEMSJ(P_DESCONECTARSE_MUERTE)			\
//		TYPEMSJ(P_DESCONECTARSE_FINALIZADO)		\
//		TYPEMSJ(P_SOLICITUD_RECURSO)			\
//		TYPEMSJ(P_FIN_TURNO)					\
//		TYPEMSJ(P_FIN_PLAN_NIVELES)				\
//		TYPEMSJ(DESCONEXION)					\
//		TYPEMSJ(NO_SE_OBTIENE_RESPUESTA)		\
//
//#define GENERATE_ENUM(ENUM) ENUM,
//#define GENERATE_STRING(STRING) #STRING,
//
//typedef enum TYPEMSJ_ENUM {
//    FOREACH_TYPEMSJ(GENERATE_ENUM)
//} tMensaje;
//
//static const char *ENUM_TO_STRING[] = {
//    FOREACH_TYPEMSJ(GENERATE_STRING)
//};
//
//
//void printAllEnums();
//int enumSize();
//const char * enumToString(int enumIndex);
//static _Bool _existEnum(int i);
//
//
//#endif /* CACA_H_ */
