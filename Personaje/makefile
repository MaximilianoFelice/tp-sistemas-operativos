# 
# Makefile personaje
#
export LD_LIBRARY_PATH=/home/utnso/tp-2013-2c-c-o-no-ser/Shared-Library/Debug/
CC		  := gcc
CFLAGS    := -Wall -L/usr/lib
LIBRARIES := -lso-commons -lginyu -lpthread
DEBUG	  := -g

personaje: personaje.o
	$(CC) $(CFLAGS) $(HEADERS) personaje.o -o personaje $(LIBRARIES)

personaje.o: personaje.c
	$(CC) $(CFLAGS) $(HEADERS) -c personaje.c $(LIBRARIES)

# Clean
clean:
	rm -f personaje.o
