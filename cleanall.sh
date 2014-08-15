#!/bin/bash
# -*- ENCODING: UTF-8 -*-

echo "Limpiando Shared Libraries... "
cd Shared-Library/Release
make clean
cd ../..
echo "DONE!"
echo ""
echo ""
echo "Limpiando Personaje... "
cd Personaje/Release
make clean
cd ../..
echo "DONE!"
echo ""
echo ""
echo "Limpiando Plataforma... "
cd Plataforma/Release
make clean
cd ../..
echo "DONE!"
echo "" 
echo ""
echo "Limpiando Nivel... "
cd Nivel/Release
make clean
cd ../..
echo "DONE!"
echo ""
echo ""
echo "Limpiando FileSystem... "
cd FileSystem/Release
make clean
cd ../..
echo "DONE!"
