###
# Makefile Shared library de la catedra
###

CC 			 := gcc
FLAGS_SHARED := -Wall -fPIC
INSTALL_DIR						= /usr/lib
INSTALL_DIR_INCLUDE_COMMONS     = /usr/include/commons
INSTALL_DIR_INCLUDE_COLLECTIONS = /usr/include/commons/collections

compile:
	$(CC) -Wall -fPIC -c *.c collections/*.c
	$(CC) -shared -Wl,-soname,libso-commons.so.1 *.o -o libso-commons.so.1.0

install: compile
	sudo mkdir -p $(INSTALL_DIR)
	sudo mv libso-commons.so.1.0 $(INSTALL_DIR)
	sudo ln -sf $(INSTALL_DIR)/libso-commons.so.1.0 $(INSTALL_DIR)/libso-commons.so.1
	sudo ln -sf $(INSTALL_DIR)/libso-commons.so.1.0 $(INSTALL_DIR)/libso-commons.so
	sudo mkdir -p  $(INSTALL_DIR_INCLUDE_COMMONS)
	sudo cp -u *.h $(INSTALL_DIR_INCLUDE_COMMONS)
	sudo mkdir -p  $(INSTALL_DIR_INCLUDE_COLLECTIONS)
	sudo cp -u collections/*.h $(INSTALL_DIR_INCLUDE_COLLECTIONS)
	sudo chmod -R 777 $(INSTALL_DIR_INCLUDE_COMMONS)
	@#export LD_LIBRARY_PATH=/opt/lib:$LD_LIBRARY_PATH

	@echo "\n"
	@echo "**********************************************************"
	@echo "*               Se crean shared libraries                *"
	@echo "**********************************************************"
	@echo "\n"

uninstall:
	sudo rm -f $(INSTALL_DIR)/libso-commons.so.1
	sudo rm -f $(INSTALL_DIR)/libso-commons.so
	sudo rm -f $(INSTALL_DIR)/libso-commons.so.1.0
	sudo rm -rf $(INSTALL_DIR_INCLUDE_COMMONS)

clean:
	rm -f *.o