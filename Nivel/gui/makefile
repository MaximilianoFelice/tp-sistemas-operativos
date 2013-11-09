### 
# Makefile Nivel
###

# Completar con sus paths
COMMONS_DIR_HEADERS=/usr/include/commons
COMMONS_DIR_BIN=/usr/lib/
COMMONS_NAME=so-commons

NIVEL_SO=libnivel-gui.so
INSTALL_DIR=/usr/lib
INSTALL_DIR_INCLUDE=/usr/include/gui

compile:
	gcc -shared -fPIC tad_items.c nivel.c -I$(COMMONS_DIR_HEADERS) -L$(COMMONS_DIR_BIN) -o $(NIVEL_SO) -l$(COMMONS_NAME)

install: compile
	sudo mkdir -p $(INSTALL_DIR)
	sudo mv $(NIVEL_SO) $(INSTALL_DIR)
	sudo mkdir -p $(INSTALL_DIR_INCLUDE)
	sudo cp -u *.h $(INSTALL_DIR_INCLUDE)
	sudo chmod -R 777 $(INSTALL_DIR_INCLUDE)
	
uninstall:
	sudo rm -f $(INSTALL_DIR)/$(NIVEL_SO)
	sudo rm -f $(INSTALL_DIR_INCLUDE)/nivel.h
	sudo rm -f $(INSTALL_DIR_INCLUDE)/tad_items.h
	
clean:
	sudo rm -f $(INSTALL_DIR)/$(NIVEL_SO)
	sudo rm -rf $(INSTALL_DIR_INCLUDE)

# PHONY targets son los que no producen un archivo llamado como el target
# Se los marca para evitar confusion si se crea un archivo llamado asi
.PHONY: compile install uninstall clean
