### 
# Makefile Ginyu Shared libraries
###

CC := gcc
FLAGSSHARED  := -Wall -fPIC
LIBRARIES	 := -lso-commons

GINYU_DIR_HEADERS=/usr/include/ginyu
GINYU_DIR_BIN=/usr/lib

compile:
	$(CC) $(FLAGSSHARED) -L$(GINYU_DIR_BIN) -c *.c $(LIBRARIES)
	$(CC) -shared -Wl,-soname,libginyu.so.1 *.o  -o libginyu.so.1.0

install: compile
	sudo mkdir -p $(GINYU_DIR_BIN)
	sudo mv libginyu.so.1.0 $(GINYU_DIR_BIN)
	sudo ln -sf $(GINYU_DIR_BIN)/libginyu.so.1.0 $(GINYU_DIR_BIN)/libginyu.so.1
	sudo ln -sf $(GINYU_DIR_BIN)/libginyu.so.1.0 $(GINYU_DIR_BIN)/libginyu.so
	sudo mkdir -p $(GINYU_DIR_HEADERS)
	sudo cp -u *.h $(GINYU_DIR_HEADERS)
	sudo chmod -R 777 $(GINYU_DIR_HEADERS)
	
	@echo "\n"
	@echo "**********************************************************"
	@echo "*            Se crean shared libraries Ginyu             *"
	@echo "**********************************************************"
	@echo "\n"

uninstall:
	sudo rm -f $(GINYU_DIR_BIN)/libginyu.so.1
	sudo rm -f $(GINYU_DIR_BIN)/libginyu.so
	sudo rm -f $(GINYU_DIR_BIN)/libginyu.so.1.0
	sudo rm -rf $(GINYU_DIR_HEADERS)

clean:
	rm -f *.o
