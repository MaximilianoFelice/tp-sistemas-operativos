__FileSystem Readme__
=====================

_Mount Parameters_
------------------

El montaje de Grasa utiliza la siguiente sintaxis:

./FileSystem [OPTIONS] [MOUNTPOINT]

MOUNTPOINT es una carpeta vacía, en donde se montará el FS.

Además, la implementación de Grasa permite la inclusión de varios parámetros de montaje. Ellos son:

--Disc-Path=[RUTA DISCO]	- Settea la ruta donde se encuentra el disco (previamente formateado en GRASA). Por defecto, se encuentra la ruta "/home/utnso/tp-2013-2c-c-o-no-ser/FileSystem/Testdisk/disk.bin".

--ll=[LOG_LEVEL]			- Define el nivel del log sobre el cual se trabajará. Los valores posibles son: ERROR, WARNING, INFO, DEBUG, TRACE y LEVEL_TRACE. Por defecto el Log Level es TRACE.

--Log-Path=[LOG_PATH]		- Settea la ruta donde se registrará el log. Solamente se indica la carpeta, se crean los archivos de loggeo de manera automática. Por defecto la rura es "/home/utnso/tp-2013-2c-c-o-no-ser/FileSystem/log/".


Parámetros propios de FUSE:

-h							- Imprime los contenidos de ayuda de FUSE.

-d 							- Abre el FileSystem en modo Debug.

-s 							- Abre el FileSystem en modo Single Thread.

-V							- Imprime la versión de FUSE que se está utilizando.


_Ejemplo_
---------

La sentencia:

    ./FileSystem -d --Disc-Path=/home/utnso/disc.bin --ll=Lock_Trace --Log-Path=/home/utnso/logs/ /tmp/fusea/
    
Monta el FileSystem, en modo DEBUG en la carpeta /tmp/fusea/, con los parámetros indicados.


_Scripts y Utilitarios_
-----------------------

La carpeta scripts y utilitarios contiene scripts que facilitan los montajes y desmontajes de FUSE.


_Creando un disco_
------------------

Esta sección contiene un breve resumen de lo indicado en los repositorios correspondientes de la cátedra. Revisar cada 
proyecto por separado para conocer a fondo cada herramienta.

* 'grasa-format': Formatea el disco que se le pasa por parámetro.

* 'grasa-dump': Genera un dump mostrando estructuras administrativas, bitmap, carpetas y archivos, con su correspondientes nodos punteros.

* 'koopa-x86 / koopa-x64': Se debe especificar un punto de montaje y un script que maneje archivos. Se puede utilizar la opción --text para saltear la interfaz gráfica.

IMPORTANTE:
	Recordar que para crear un nuevo disco, se debe utilizar un comando al estilo:
    dd if=/dev/urandom of=disco.bin bs=1024 count=10240
