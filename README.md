C o no ser, esa es la cuestion...
=====================
Implementacion del TP de sistemas operativos
----------------------------------------------------------

### Cosirijillas:

Los tipos de mensajes, los paquetes a enviar y la implementeacion de la serializacion y deserialización de estos se encuentran en: - [protocolo.h](https://github.com/sisoputnfrba/tp-2013-2c-c-o-no-ser/blob/master/Shared-Library/ginyu/protocolo.h)

----------------------------------------------------------

### Pasos para ejecutar los procesos:

Ir a la carpeta de cada proceso. Y ejecutar:

`./mountplatform.sh`

`./mountlevel.sh [Level_Config]`

`./mountcharacter.sh [Level_Config]`

----------------------------------------------------------

### A manopla::

Primero: `export LD_LIBRARY_PATH=/home/utnso/tp-2013-2c-c-o-no-ser/Shared-Library/Debug/`

Luego usando los ejecutables compilados por Eclipse, en este orden, los procesos:

`./Debug/plataforma plataforma.config -v -ll trace`

`./Debug/nivel [Level_Config]`

`./Debug/personaje [Level_Config] -v`

----------------------------------------------------------

### Usando nuestros makefiles:

Primero compilamos la gui (ubicada dentro de la carpeta de nivel), y las shared libraries commons y ginyu, para eso, una vez posicionados en las respectivas carpetas hacen un:

`make compile`

para compilar los archivos de C, y después:

`make install`

Que copia las librerias al /usr/lib y los headers al /usr/include

Despues para compilar cada componente individual, van a la carpeta correspondiente de cada uno (Nivel, Plataforma, Personaje) y hacen un:

`make`

Para ejecutarlos:

`./plataforma plataforma.config -v`

`./nivel [Level_Config]`

`./personaje [Level_Config] -v`


----------------------------------------------------------

### Filesystem:

Revisar el readme ubicado en: https://github.com/sisoputnfrba/tp-2013-2c-c-o-no-ser/tree/master/FileSystem

