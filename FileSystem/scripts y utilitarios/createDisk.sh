#!/bin/bash
# -*- ENCODING: UTF-8 -*-

# Contemplamos la ayuda
if [[ $1 == "--help" ]]
then
	echo "	Usage:"
	echo "	./createDisk [MB] [NAME]"
	echo ""
	echo "		MB	-	Amount of MB that the disk will have. By default, this argument is 10"
	echo "		NAME	-	Name that will be given to the disk. This argument, by default, will create a disk named disk.bin"
	echo ""
	echo "		For example: ./createDisk 100 grasaDisk.bin"
	echo ""
	echo "	This means that by default, this script will create and format with GRASA a 10MB disk, named disk.bin"
	echo ""
	echo "	You could also use --clean [NAME] to delete a disk named NAME. If NAME is not specified, it will try to delete disk.bin"
	echo " 	For example ./createDisk --clean disk.bin , which is the default option."
	echo ""
	echo ""
	echo "						Maximiliano Felice."
	echo "						Ingenieria en Sistemas de Informacion - UTN - FRBA "
	echo ""
	echo ""
	exit
fi

if [[ $2 != "" ]]
then
        name=$2
else
        name="disk.bin"
fi

if [[ $1 == "--clean" ]]
then
	if [[ $2 != "" ]]
	then
		rm $2
	else
		rm disk.bin
	fi

	exit
elif [[ $1 == "--format" ]]
then
	echo -n "Formatting &path ...	"
	./grasa-format $path
	echo "DONE!"
	exit
fi

((mb=1024*1024))
if [[ $1 != "" ]]
then
	cant=$1
else
	cant=10
fi

echo "Creating $name of $cant MB"
dd if=/dev/urandom of=$name bs=$mb count=$cant
echo "Formatting with GRASA"
./grasa-format $name
echo "Formatted and ready to go!!!"
echo ""
