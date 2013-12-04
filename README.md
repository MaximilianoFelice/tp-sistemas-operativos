# C o no ser, esa es la cuestion...
=====================
Implementacion del TP de sistemas operativos
----------------------------------------------------------

### Cosirijillas:

Los tipos de mensajes, los paquetes a enviar y la implementeacion de la serializacion y deserialización de estos se encuentran en: - [protocolo.h](https://github.com/sisoputnfrba/tp-2013-2c-c-o-no-ser/blob/master/Shared-Library/ginyu/protocolo.h)

----------------------------------------------------------

### Pasos para ejecutar los procesos:

Ir a la carpeta de cada proceso. Y ejecutar:

#### `./mountplatform`

#### `./mountlevel (archivo de configuracion del nivel)`

#### `./mountcharacter (archivo de configuracion del personaje)`

----------------------------------------------------------

### Con los makefiles de Eclipse, asi:

Primero: `export LD_LIBRARY_PATH=/home/utnso/tp-2013-2c-c-o-no-ser/Shared-Library/Debug/`

Luego ejecutar, en este orden, los procesos:

`./Debug/plataforma plataforma.config -v -ll trace`

`./Debug/nivel (archivo de configuracion del nivel)`

`./Debug/personaje (archivo de configuracion del personaje) -v`

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

`./nivel (archivo de configuracion del nivel)`

`./personaje (archivo de configuracion del personaje) -v`

