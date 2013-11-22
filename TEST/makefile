###
# Makefile TEST
###

CC	  := gcc
CFLAGS    := -Wall -L/usr/lib
LIBRARIES := -lso-commons -lginyu
DEBUG	  := -g

cliente: cliente.o
	$(CC) $(CFLAGS) $(HEADERS) cliente.o -o cliente $(LIBRARIES)

servidor: servidor.o
	$(CC) $(CFLAGS) $(HEADERS) servidor.o -o servidor $(LIBRARIES)

cliente.o: cliente.c
	$(CC) $(CFLAGS) $(HEADERS) -c cliente.c $(LIBRARIES)

servidor.o: servidor.c
	$(CC) $(CFLAGS) $(HEADERS) -c servidor.c $(LIBRARIES)

# Clean
clean:
	rm -f *.o
