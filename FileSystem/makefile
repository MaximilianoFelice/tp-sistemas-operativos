###
# Makefile FileSystem
###

CC        := gcc
CFLAGS    := -Wall -L/usr/lib/
LIBRARIES := -lso-commons -lginyu -lpthread -lfuse
DEBUG	  := -g

MFILESYSTEM = FileSystem.o Grasa_Handler.o Grasa_Read.o Grasa_Write.o

filesystem: $(MFILESYSTEM)
	$(CC) $(DEBUG) $(CFLAGS) $(HEADERS) $(MFILESYSTEM) -o filesystem $(LIBRARIES)

FileSystem.o: FileSystem.c
	$(CC) $(CFLAGS) $(HEADERS) -c FileSystem.c $(LIBRARIES)

Grasa_Handler.o: Grasa_Handler.c
	$(CC) $(CFLAGS) $(HEADERS) -c Grasa_Handler.c $(LIBRARIES)

Grasa_Read.o: Grasa_Read.c
	$(CC) $(CFLAGS) $(HEADERS) -c Grasa_Read.c $(LIBRARIES)

Grasa_Write.o: Grasa_Write.c
	$(CC) $(CFLAGS) $(HEADERS) -c Grasa_Write.c $(LIBRARIES)

# Clean
clean:
	rm -f nivel.o
