#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>

#define MAX_ROWS 100
#define MAX_COLS 100
#define MAX_THREADS 500
#define MOVE_DELAY_US 500000 // 5000 milisegundos (0.5 segundos) de pausa entre movimientos de hilo
#define MOVE_DELAY_PRINT 1000000 

// Estructura para representar el laberinto
typedef struct {
    char cells[MAX_ROWS][MAX_COLS]; // Matriz de celdas del laberinto
    int rows; // Número de filas del laberinto
    int cols; // Número de columnas del laberinto
} Maze;

// Estructura para datos de un hilo
typedef struct {
    int row; // Fila actual del hilo
    int col; // Columna actual del hilo
} Position;

typedef struct {
    Position positions[MAX_THREADS]; // Posiciones de los hilos
    int directions[MAX_THREADS]; // Direcciones de los hilos
    int num_threads; // Número actual de hilos
    int visited[MAX_THREADS][MAX_ROWS][MAX_COLS]; // Registro de posiciones visitadas por cada hilo
    int thread_directions[MAX_ROWS][MAX_COLS][4]; // Registro de direcciones de los hilos que pasaron por cada posición
    pthread_mutex_t mutex; // Mutex para sincronización
    int spaces_traversed[MAX_THREADS]; // Variable para contar los espacios recorridos por cada hilo
} ThreadData;

// Variables globales
Maze maze; // Laberinto
ThreadData thread_data = {0}; // Datos compartidos entre hilos, inicializados a 0
int hilos=0; 
int salida=0;

// Declaración de la función para crear un nuevo hilo si es posible y empezar a moverlo
void create_thread_if_possible(int row, int col, int direction, int accumulated_spaces );

// Función para asignar un nuevo ID de hilo
int allocate_thread_id() {
    static int next_thread_id = 0;
    static int thread_id_lock = 0;
    int id = -1;
    
    // Utilizar un lock para asegurar que la asignación de ID sea atómica
    while (__sync_lock_test_and_set(&thread_id_lock, 1) != 0);
    
    if (next_thread_id < MAX_THREADS) {
        id = next_thread_id++;
    }
    // Liberar el lock
    __sync_lock_release(&thread_id_lock);
    return id;
}

// Función para mover un hilo en una dirección dada
void move_thread(int thread_id, int dx, int dy) {
    pthread_mutex_lock(&thread_data.mutex);
    thread_data.positions[thread_id].row += dx;
    thread_data.positions[thread_id].col += dy;
    pthread_mutex_unlock(&thread_data.mutex);
}

// Función para verificar si una posición es válida en el laberinto
int is_valid_position(Position pos) {
    return pos.row >= 0 && pos.row < maze.rows && pos.col >= 0 && pos.col < maze.cols;
}

// Función para verificar si una posición es una pared en el laberinto
int is_wall(Position pos) {
    return maze.cells[pos.row][pos.col] == '*';
}

// Función para verificar si una posición es una pared en el laberinto
int is_meta(Position pos) {
    return maze.cells[pos.row][pos.col] == '/';
}

// Mutex para controlar la impresión del laberinto
pthread_mutex_t print_mutex = PTHREAD_MUTEX_INITIALIZER;

// Función para imprimir el laberinto con las posiciones de los hilos
void print_maze_with_thread_positions() {
    pthread_mutex_lock(&print_mutex); // Bloquear el acceso a la función de impresión
    printf("\n\n\n\n\n\n\n");
    printf("__________Laberinto__________\n\n");
    printf("   ");
    for (int c = 0; c < maze.cols; c++) {
        printf("%d ", c);
    }
    printf("\n");

    char maze_representation[MAX_ROWS][MAX_COLS]; // Representación del laberinto con las posiciones de los hilos
    memcpy(maze_representation, maze.cells, sizeof(maze.cells)); 
    
    // Marcar todas las posiciones de los hilos en la representación del laberinto
    for (int k = 0; k < thread_data.num_threads; k++) {
        int i = thread_data.positions[k].row;
        int j = thread_data.positions[k].col;
        char thread_dir = ' ';
        switch(thread_data.directions[k]) {
            case 0: thread_dir = '^'; maze.cells[i][j] = '^'; break; // Arriba
            case 1: thread_dir = '>'; maze.cells[i][j] = '>'; break; // Derecha
            case 2: thread_dir = 'v'; maze.cells[i][j] = 'v'; break; // Abajo
            case 3: thread_dir = '<'; maze.cells[i][j] = '<'; break; // Izquierda
        }
        maze_representation[i][j] = thread_dir;
    }
    
    // Imprimir el laberinto con todas las posiciones de los hilos y las coordenadas de las filas
    for (int i = 0; i < maze.rows; i++) {
        // Imprimir número de fila
        printf("%2d ", i);
        
        for (int j = 0; j < maze.cols; j++) {
            printf("%c ", maze_representation[i][j]);
        }
        printf("\n");
    }
    printf("_______Informe_de_hilos______\n");
    pthread_mutex_unlock(&print_mutex); // Liberar el acceso a la función de impresión
}

void move_thread_in_one_direction(int thread_id) {
    int dx, dy;
    switch (thread_data.directions[thread_id]) {
        case 0: // Arriba
            dx = -1; dy = 0;
            break;
        case 1: // Derecha
            dx = 0; dy = 1;
            break;
        case 2: // Abajo
            dx = 1; dy = 0;
            break;
        case 3: // Izquierda
            dx = 0; dy = -1;
            break;
        default:
            dx = 0; dy = 0;
    }

    int next_row = thread_data.positions[thread_id].row;
    int next_col = thread_data.positions[thread_id].col;
    
    int salir=1;
    // Mover el hilo en línea recta hasta encontrar una pared
    while (salir) {
    	
        usleep(MOVE_DELAY_US);
    	print_maze_with_thread_positions();
        // Pausa entre movimientos de hilo

    	if (dy == 0) { // Movimiento vertical
	    create_thread_if_possible( next_row, next_col + 1, 1, thread_data.spaces_traversed[thread_id]);
	    create_thread_if_possible( next_row, next_col - 1, 3, thread_data.spaces_traversed[thread_id]);
	
	} else { // Movimiento horizontal
    	    create_thread_if_possible( next_row + 1, next_col, 2, thread_data.spaces_traversed[thread_id]);
	    create_thread_if_possible(next_row - 1, next_col, 0, thread_data.spaces_traversed[thread_id]);    
	}
	    	 
        // Actualizar el registro de posiciones visitadas por este hilo
        pthread_mutex_lock(&thread_data.mutex);
        thread_data.visited[thread_id][next_row][next_col] = 1;
        pthread_mutex_unlock(&thread_data.mutex);
	   
	next_row = thread_data.positions[thread_id].row + dx;
        next_col = thread_data.positions[thread_id].col + dy;
    
        if (!is_valid_position((Position){next_row, next_col}) || is_wall((Position){next_row, next_col})) {
            pthread_mutex_unlock(&thread_data.mutex);
            break;
        }
        
        // Verificar si la posición ya ha sido visitada por otro hilo en la misma dirección
        if (thread_data.thread_directions[next_row][next_col][thread_data.directions[thread_id]] == 1 ) {
            pthread_mutex_lock(&print_mutex); // Bloquear el acceso a la función de impresión
            printf("->Hilo %d laberinto ya explorada en la misma direccion en la posicion (%d,%d), recorrio %d espacios.<-\n", thread_id,  next_col, next_row, thread_data.spaces_traversed[thread_id]);
            usleep(MOVE_DELAY_PRINT);
            pthread_mutex_unlock(&print_mutex); // Liberar el acceso a la función de impresión
            hilos--;
            salir=0;
            break;
        } 
        
        // Verificar si la posición actual es la meta
        if (is_meta((Position){next_row, next_col})) {
            pthread_mutex_lock(&print_mutex); 
            printf("->Hilo %d ha alcanzado la meta en la posicion (%d,%d) después de recorrer %d espacios.<-\n", thread_id, next_col, next_row, thread_data.spaces_traversed[thread_id]+1);
            usleep(MOVE_DELAY_PRINT);
            pthread_mutex_unlock(&print_mutex); 
            salir=0;
            hilos--;
            salida++;
            break;
        }

        // Marcar la nueva posición como visitada
        thread_data.visited[thread_id][next_row][next_col] = 1;
        pthread_mutex_unlock(&thread_data.mutex);
	    // Actualizar el registro de direcciones de los hilos que pasaron por esta posición
        pthread_mutex_lock(&thread_data.mutex);
        thread_data.thread_directions[next_row][next_col][thread_data.directions[thread_id]] = 1;
        pthread_mutex_unlock(&thread_data.mutex);
        // Mover el hilo a la nueva posición
        move_thread(thread_id, dx, dy);
        thread_data.spaces_traversed[thread_id]++; // Incrementar el contador de espacios recorridos para este hilo
    }
    	 
	// Verificar si la posición actual es una pared
	if (is_wall((Position){next_row, next_col}) || !is_valid_position((Position){next_row, next_col}) ) {
		pthread_mutex_lock(&print_mutex); // Bloquear el acceso a la función de impresión
		printf("->Hilo %d ha alcanzado una pared posicion (%d,%d) después de recorrer %d espacios.<-\n", thread_id, next_col, next_row, thread_data.spaces_traversed[thread_id]);
		usleep(MOVE_DELAY_PRINT);
		pthread_mutex_unlock(&print_mutex); // Liberar el acceso a la función de impresión
		salir=0;
		hilos--;
	}
}

void create_thread_if_possible(int row, int col, int direction,int accumulated_spaces) {
    int can_create_thread = 1; // Variable para indicar si se puede crear un nuevo hilo
    
    if (thread_data.num_threads < MAX_THREADS && is_valid_position((Position){row, col})) {
        pthread_mutex_lock(&thread_data.mutex);

        // Verificar si la posición ya ha sido visitada por otro hilo en la misma dirección
        if (thread_data.thread_directions[row][col][direction] == 1 ) {
            can_create_thread = 0; // No se puede crear un nuevo hilo en esta dirección  
        } 

        // Verificar si la nueva posición está vacía (no es una pared)
        if (is_wall((Position){row, col})) {
            can_create_thread = 0; // No se puede crear un nuevo hilo en esta dirección
  
        }  
        // Verificar si la posición del nuevo hilo es la meta
        if (is_meta((Position){row, col})) {
       	    pthread_mutex_lock(&print_mutex); 
            printf("->Hilo ha alcanzado la meta en la posicion (%d,%d) después de recorrer %i espacios.<-\n",col,row, accumulated_spaces );
            usleep(MOVE_DELAY_PRINT);
            pthread_mutex_unlock(&print_mutex); 
            can_create_thread = 0;
            salida++;
         }
                  
        pthread_mutex_unlock(&thread_data.mutex);

        // Crear el nuevo hilo 
        if (can_create_thread) {
       
            pthread_t thread;
            int thread_id = allocate_thread_id(); // Obtener un nuevo ID de hilo    
            thread_data.spaces_traversed[thread_id] = accumulated_spaces;
            
            if (thread_id != -1) {
           
                    pthread_mutex_lock(&thread_data.mutex);
                    thread_data.positions[thread_id].row = row;
                    thread_data.positions[thread_id].col = col;
                    thread_data.directions[thread_id] = direction;
                    thread_data.num_threads++;
                    pthread_mutex_unlock(&thread_data.mutex);

                    // Actualizar el registro de direcciones de los hilos que pasaron por esta posición
                    pthread_mutex_lock(&thread_data.mutex);
                    thread_data.thread_directions[row][col][direction] = 1;
                    pthread_mutex_unlock(&thread_data.mutex);
                    pthread_create(&thread, NULL, (void *(*)(void *))move_thread_in_one_direction, (void *)(intptr_t)thread_id); // Crear el nuevo hilo
                    hilos++;
                    thread_data.spaces_traversed[thread_id]++;
                    pthread_detach(thread); // Desconectar el hilo para liberar recursos automáticamente al finalizar
               
            } else {
                fprintf(stderr, "Error: No se pudo asignar un nuevo ID de hilo\n");
                exit(EXIT_FAILURE);
            }
        }
    }
}


// Función principal
int main() {
    // Inicializar el laberinto leyendo desde el archivo de texto
    FILE *file = fopen("laberinto.txt", "r");
    if (file == NULL) {
        perror("Error al abrir el archivo");
        exit(EXIT_FAILURE);
    }

    if (fscanf(file, "%d %d", &maze.cols, &maze.rows) != 2) {
        fprintf(stderr, "Error: No se pudo leer las dimensiones del laberinto del archivo\n");
        exit(EXIT_FAILURE);
    }

    printf("Dimensiones del laberinto: %d filas x %d columnas\n", maze.rows, maze.cols);

    // Leer el carácter de nueva línea después de las dimensiones
    fgetc(file);
    
    for (int i = 0; i < maze.rows; i++) {
        if (fgets(maze.cells[i], sizeof(maze.cells[i]), file) == NULL) {
            fprintf(stderr, "Error: No se pudo leer la fila %d del laberinto del archivo\n", i + 1);
            exit(EXIT_FAILURE);
        }
    }
    print_maze_with_thread_positions();

    fclose(file);

    // Inicializar el mutex
    pthread_mutex_init(&thread_data.mutex, NULL);
    thread_data.directions[0] = 2; // Dirección inicial: Abajo
    // Crear el primer hilo en la posición inicial (0, 0) con dirección hacia abajo (2) espacios caminados -1
    create_thread_if_possible(0, 0, 1, -1);

    // Esperar hasta que todos los hilos hayan terminado
    while (hilos > 0) {
        usleep(1000); 
    }
    // Valida la cantidad de caminos disponibles a la salida
    if (salida>0){
    	if(salida==1){
		printf("\n\nHay un solo camino que conduce a la salida \n\n");
    	}
    	else{
             printf("\n\nHay %i caminos diferentes que conducen a la salida \n\n", salida);
    	}	
    }
    else{
    	printf("\n\nNo se encontro una salida del laberinto!!! :O \n\n");
    }

    return 0;
}
