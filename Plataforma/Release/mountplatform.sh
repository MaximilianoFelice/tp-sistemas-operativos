#!/bin/bash
# -*- ENCODING: UTF-8 -*-

export LD_LIBRARY_PATH=../../Shared-Library/Release/
if [ "$1" == "--valgrind" ]
then
echo "Debugging with valgrind"
valgrind --leak-check=full --track-origins=yes --show-reachable=yes ./plataforma ../plataforma.config -v
else
./plataforma ../plataforma.config -v
fi
