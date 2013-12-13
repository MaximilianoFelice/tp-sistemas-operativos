#!/bin/bash
# -*- ENCODING: UTF-8 -*-

search="personaje"
if [[ $1 == "--list" ]]
then
	#Cambia el parametro de busqueda definido por el usuario
	if [[ $2 != "" ]]
	then
		search=$2
	fi

	echo "$(ps -fea | grep $search)"
else
	if [[ $1 != "" ]] 
	then
		search=$1
	fi

	pid=$(pidof personaje)

	if [ pid == "" ]
	then
		echo "No existe el proceso personaje"
	else
		kill -SIGTERM $pid
	fi
fi
