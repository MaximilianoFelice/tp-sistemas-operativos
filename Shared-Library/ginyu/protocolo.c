
#include "protocolo.h"

message_t armarMsj_return(char name, int8_t type, int8_t detail, int8_t detail2) {

	message_t msj;
	if(name != 0)
		msj.name = name;
	if(type != 0)
		msj.type = type;
	if(detail!=0)
		msj.detail = detail;
	if(detail2 != 0)
		msj.detail2 = detail2;
	return msj;

}

void armarMsj(message_t *msj, char name, int8_t type, int8_t detail, int8_t detail2){

	if(name != 0)
		msj->name = name;
	if(type != 0)
		msj->type = type;
	if(detail!=0)
		msj->detail = detail;
	if(detail2 != 0)
		msj->detail2 = detail2;

}

orq_t armarOrqMsj_return(char *name, int8_t type, int8_t detail, char *ip, int port){

	orq_t msj;
	if(name != 0)
		strcpy(msj.name, name);
	if(type != 0)
		msj.type = type;
	if(detail!=0)
		msj.detail = detail;
	if(ip != 0)
		strcpy(msj.ip, strdup(ip));
	if(port !=0)
		msj.port = port;
	return msj;
}

void armarOrqMsj(orq_t *msj, char *name, int8_t type, int8_t detail, char *ip, int port){

	if(name != 0)
		strcpy(msj->name, name);
	if(type != 0)
		msj->type = type;
	if(detail!=0)
		msj->detail = detail;
	if(ip != 0)
		strcpy(msj->ip, ip);
	if(port !=0)
		msj->port = port;

}
