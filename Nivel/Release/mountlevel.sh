#!/bin/bash
# -*- ENCODING: UTF-8 -*-

export LD_LIBRARY_PATH=../../Shared-Library/Release/
if [ "$2" == "--valgrind" ]
then
echo "Debugging with valgrind"
valgrind --leak-check=full --track-origins=yes --show-reachable=yes --log-file=logvalgrind.txt ./nivel $1
else
./nivel $1
fi

