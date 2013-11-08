#!/bin/bash
# -*- ENCODING: UTF-8 -*-

fuser -u /tmp/fusea/
rm -r /tmp/fusea
mkdir /tmp/fusea
./tp-2013-2c-c-o-no-ser/FileSystem/Debug/FileSystem -d --ll=Error --Disc-Path=/home/utnso/tp-2013-2c-c-o-no-ser/FileSystem/Testdisk/discoGrande.bin /tmp/fusea/
