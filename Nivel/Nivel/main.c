
#include <nivel-gui/tad_items.h>
#include <stdlib.h>
#include <stdio.h>
#include <curses.h>
#include <commons/collections/list.h>
#include <commons/txt.h>
#include <commons/config.h>
#include <commons/string.h>

/*
 * @NAME: rnd
 * @DESC: Modifica el numero en +1,0,-1, sin pasarse del maximo dado
 */

void rnd(int *x, int max){
	*x += (rand() % 3) - 1;
	*x = (*x<max) ? *x : max-1;
	*x = (*x>0) ? *x : 1;
}

/*
 * @NAME: NombreNivel
 * @DESC: Devuelve el nombre del nivel segun el archivo de configuracion
 */
void NombreNivel(t_config *config, char ** nombrenivel){
	char * key = "Nombre";
	if(config_has_property(config,key)){
		*nombrenivel = config_get_string_value(config,key);
		// aca tal vez se debe validar que no exista un nivel con = nombre
	}
}

/*
 * @NAME: DibujarEnemigos
 * @DESC: Dibuja en forma aleatoria tantos enemigos como se especifique
 * en el archivo de configuracion y dentro de los margenes de la pantalla
 */
void DibujarEnemigos(t_list* items, t_config *config, int rows, int cols){
	int enemigos,e,x,y;
	char * key = "Enemigos";
	char * id;


	if (config_has_property(config,key)){
		enemigos = config_get_int_value(config,key);
		e = 1;
		while(e<=enemigos){
			id = string_from_format("%d",e);

			x= rand()%cols;
			y= rand()%rows;

			CrearEnemigo(items, id[0], x, y);
			e++;
		}

	}

}
/*
 * @NAME: DibujarCajas
 * @DESC: Dibuja las cajas de recursos especificadas en el archivo de
 * configuracion
 */
void DibujarCajas(t_list* items, t_config *config) {

	char * key;
	char  simbolo;
	char * cnum;
	char * string;
	char** valores;
	int posx, posy, instancias;
	int n = 1;
	key = "Caja1";

	while (config_has_property(config,key)) {
		// agarrar cada cosa
		valores = config_get_array_value(config, key);
		simbolo = valores[1][0]; // simbolo

		instancias = atoi(valores[2]); // Instancias
		posx = atoi(valores[3]); // pos x
		posy = atoi(valores[4]); // pos y
			CrearCaja(items, simbolo, posx,posy,instancias);
			n++;
			string = "Caja";
			cnum =string_from_format("%d",n);
			key= malloc(snprintf(NULL, 0, "%s%s", string, cnum) + 1);
			sprintf(key, "%s%s", string, cnum);
		}
}

int main(void) {

    t_list* items = list_create();

	int rows, cols;
	int q, p;

	int x = 1;
	int y = 1;

	int ex1 = 10, ey1 = 14;
	int ex2 = 20, ey2 = 3;


	t_config* config;
	char *path = "/home/utnso/workspace/usando-nivelGui/conf/nivel.conf";
	char *nombreNivel;


	nivel_gui_inicializar();

    nivel_gui_get_area_nivel(&rows, &cols);

	p = cols;
	q = rows;

	CrearPersonaje(items, '@', p, q);
	CrearPersonaje(items, '#', x, y);
	/*
	CrearEnemigo(items, '1', ex1, ey1);
	CrearEnemigo(items, '2', ex2, ey2);*/

	config = config_create(path); /*Levantamos archivo de configuracion*/
	DibujarEnemigos(items,config,rows,cols); /*Dibujamos enemigos a partir del archivo*/
	DibujarCajas(items, config);  /*Dibujamos las cajas a partir del archivo*/
	NombreNivel(config, &nombreNivel);
	//nivel_gui_dibujar(items, "Test Chamber 04");
	nivel_gui_dibujar(items, nombreNivel);

	while ( 1 ) {
		int key = getch();

		switch( key ) {

			case KEY_UP:
				if (y > 1) {
					y--;
				}
			break;

			case KEY_DOWN:
				if (y < rows) {
					y++;
				}
			break;

			case KEY_LEFT:
				if (x > 1) {
					x--;
				}
			break;
			case KEY_RIGHT:
				if (x < cols) {
					x++;
				}
			break;
			case 'w':
			case 'W':
				if (q > 1) {
					q--;
				}
			break;

			case 's':
			case 'S':
				if (q < rows) {
					q++;
				}
			break;

			case 'a':
			case 'A':
				if (p > 1) {
					p--;
				}
			break;
			case 'D':
			case 'd':
				if (p < cols) {
					p++;
				}
			break;

			case 'Q':
			case 'q':
				nivel_gui_terminar();
				exit(0);
			break;
		}


		rnd(&ex1, cols);
		rnd(&ey1, rows);
		rnd(&ex2, cols);
		rnd(&ey2, rows);
		MoverPersonaje(items, '1', ex1, ey1 );
		MoverPersonaje(items, '2', ex2, ey2 );

		MoverPersonaje(items, '@', p, q);
		MoverPersonaje(items, '#', x, y);

		if (   ((p == 26) && (q == 10)) || ((x == 26) && (y == 10)) ) {
			restarRecurso(items, 'H');
		}

		if (   ((p == 19) && (q == 9)) || ((x == 19) && (y == 9)) ) {
			restarRecurso(items, 'F');
		}

		if (   ((p == 8) && (q == 15)) || ((x == 8) && (y == 15)) ) {
			restarRecurso(items, 'M');	
		}

		if((p == x) && (q == y)) {
			BorrarItem(items, '#'); //si chocan, borramos uno (!)
		}

		nivel_gui_dibujar(items, "Test Chamber 04");
	}

	BorrarItem(items, '#');
	BorrarItem(items, '@');

	BorrarItem(items, '1');
	BorrarItem(items, '2');

	BorrarItem(items, 'H');
	BorrarItem(items, 'M');
	BorrarItem(items, 'F');

	nivel_gui_terminar();
}


