###
# Makefile personaje
###

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
