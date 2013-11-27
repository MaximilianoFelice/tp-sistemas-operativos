#!/bin/bash
# -*- ENCODING: UTF-8 -*-

# curl -L https://github.com/sisoputnfrba/massive-filecreator/
tarball/master -o mfc.tar.gz && tar xvfz mfc.tar.gz
# gcc massive-file-creator.c -o mfc -lcrypto -lpthread
# ./mfc 10 1024 path/al/fs/GRASA prefix_
