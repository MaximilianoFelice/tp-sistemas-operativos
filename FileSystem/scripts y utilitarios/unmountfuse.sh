#!/bin/bash
# -*- ENCODING: UTF-8 -*-

path="/tmp/fusea"

if [[ $1 == "--help" ]]
then
	echo ""
	echo "	Help of unmountfuse:"
	echo ""
	echo "	Unmountfuse will unmount a currently mounted FUSE implementation in PATH. By default, PATH is $path"
	echo ""
	echo "	Sintax: ./unmountfuse.sh [PATH]"
	echo ""
	exit
fi

if [[ $1 != "" ]]
then
	path=$1
fi

fusermount -u $path
rm -r $path
mkdir $path
