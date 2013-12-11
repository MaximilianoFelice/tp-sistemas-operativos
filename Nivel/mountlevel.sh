#!/bin/bash
# -*- ENCODING: UTF-8 -*-

export LD_LIBRARY_PATH=/home/utnso/tp-2013-2c-c-o-no-ser/Shared-Library/Debug/
if [ "$2" == "--valgrind" ]
then
echo "Debugging with valgrind"
valgrind --leak-check=full --track-origins=yes --show-reachable=yes --log-file=logvalgrind.txt ./Debug/nivel $1
else
./Debug/nivel $1
fi

