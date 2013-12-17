#!/bin/bash
# -*- ENCODING: UTF-8 -*-

echo "Realizando un make de las Shared Libraries... "
cd Shared-Library/Release
make clean
make -s
cd ../..
echo "DONE!"
echo ""
echo ""
echo "Realizando un make del Personaje... "
cd Personaje/Release
make clean
make -s
cd ../..
echo "DONE!"
echo ""
echo ""
echo "Realizando un make de Plataforma... "
cd Plataforma/Release
make clean
make -s
cd ../..
echo "DONE!"
echo "" 
echo ""
echo "Realizando un make de Nivel... "
cd Nivel/Release
make clean
make -s
cd ../..
echo "DONE!"
echo ""
echo ""
echo "Realizando un make de FileSystem... "
cd FileSystem/Release
make clean
make -s
cd ../..
echo "DONE!"
