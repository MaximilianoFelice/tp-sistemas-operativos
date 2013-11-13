###
# Makefile nivel
###

CC		  := gcc
CFLAGS    := -Wall -L/usr/lib/
LIBRARIES := -lso-commons -lginyu -lpthread -lnivel-gui -lncurses -lm
DEBUG	  := -g

nivel: nivel.o
	$(CC) $(DEBUG) $(CFLAGS) $(HEADERS) nivel.o -o nivel $(LIBRARIES)

nivel.o: nivel.c
	$(CC) $(CFLAGS) $(HEADERS) -c nivel.c $(LIBRARIES)

# Clean
clean:
	rm -f nivel.o
