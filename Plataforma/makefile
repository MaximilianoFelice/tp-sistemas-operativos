# 
# Makefile plataforma
#

CC		  := gcc
CFLAGS    := -Wall -L/usr/lib
LIBRARIES := -lso-commons -lginyu -lpthread
DEBUG	  := -g

plataforma: plataforma.o
	$(CC) $(DEBUG) $(CFLAGS) $(HEADERS) plataforma.o -o plataforma $(LIBRARIES)

plataforma.o: plataforma.c
	$(CC) $(CFLAGS) $(HEADERS) -c plataforma.c $(LIBRARIES)

# Clean
clean:
	rm -f plataforma.o