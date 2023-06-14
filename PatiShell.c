/*
UNIX Shell Project: Autor Luis Patiño Camacho
 
Sistemas Operativos
Grados I. Informatica, Computadores & Software
Dept. Arquitectura de Computadores - UMA
 
Some code adapted from "Fundamentos de Sistemas Operativos", Silberschatz et al.
 
To compile and run the program:
   $ gcc Shell_project.c job_control.c -o Shell
   $ ./Shell          
    (then type ^D to exit program) 
 */

#include "job_control.h"   // remember to compile with module job_control.c
#include <string.h>        //para poder comparar cadenas en los comandos internos
#include <stdlib.h>
#define MAX_LINE 256 /* 256 chars per line, per command, should be enough. */

// -----------------------------------------------------------------------
//                            MANEJADOR        
// -----------------------------------------------------------------------
job* tareas; //variable global para la lista de trabajos
void manejador(int sig) {
    block_SIGCHLD(); //bloquea las señales de los hijos 
    int status, info, pid_wait; // DECLARO LAS VARIABLES
    enum status estado;
    job* trabajo = tareas;
    job *aux = NULL;
    trabajo = trabajo->next;

    while (trabajo != NULL) {
        pid_wait = waitpid(trabajo->pgid, &status, WUNTRACED | WNOHANG | WCONTINUED); // pide la señal del pid selecionadao 
        estado = analyze_status(status, &info); //analiza el estado

        if (pid_wait == trabajo->pgid) { // si el pid del trabajo selecionado coincide con el que le ha mandado la señal 
            if (estado == EXITED  || estado == SIGNALED) {// analizamos el tipo de señal 
                aux = trabajo->next;
                delete_job(tareas, trabajo); // borramos el trabajo de la lista
                trabajo = aux; // selecionamos el siguiente trabajo 
            }//FIN IF  
            else if (estado == SUSPENDED) {// si el estado es suspendido 
                trabajo->state = STOPPED; // paramos el trabajo 
                trabajo = trabajo->next; // selecionamos el siguiente trabajo 
            }//FIN  ELSE IF
        }//FIN IF 
        else {
            trabajo = trabajo->next;
        }//FIN ELSE
    }//FIN WHILE 
unblock_SIGCHLD(); // desbloquea las señales de los hijos
}//FIN MANEJADOR

//COMPROBAR QUE SI QUITO EL TRABAJO -> NEXT SIGUE FUNCIONANDO CON NORMALIDAD

// -----------------------------------------------------------------------
//                            MAIN      
// -----------------------------------------------------------------------

int main(void) {

    char inputBuffer[MAX_LINE]; /* buffer to hold the command entered */
    int background; /* equals 1 if a command is followed by '&' */
    char *args[MAX_LINE / 2]; /* command line (of 256) has max of 128 arguments */
    // probably useful variables:
    int pid_fork, pid_wait; /* pid for created and waited process */
    int status; /* status returned by wait */
    enum status status_res; /* status processed by analyze_status() */
    int info; /* info processed by analyze_status() */
    int res;
    ignore_terminal_signals(); // ignoramos las señales del terminal 
    tareas = new_list("jobs_list"); //creamos la lista de trabajos 
    signal(SIGCHLD, manejador); //leemos las señales de los hijos 

// -----------------------------------------------------------------------
//                            WHILE (LOOP DEL SHELL)        
// -----------------------------------------------------------------------

    while (1) {/* Program terminates normally inside get_command() after ^D is typed */
        printf("\e[1;35m  COMMAND->\e[0m");
        fflush(stdout);
        get_command(inputBuffer, MAX_LINE, args, &background); //leemos el comando

//                            COMANDO VACIO       
// -----------------------------------------------------------------------

        if (args[0] == NULL) // comprobamos que la instruccion no este vacio
        {
            continue;
        }//FIN IF            

// -----------------------------------------------------------------------
//                            CD        
// -----------------------------------------------------------------------

        if (!strcmp(args[0], "cd")) { //comprobamos si es la instruccion interna cd
            if (args[1] == NULL) {// si no recive parametros
                chdir("/home");
                //printf( "Redirecting home\n" );
            }//FIN IF
            else if (chdir(args[1]) < 0) {// si no recive parametros
                printf( "Cant change to directory:  %s\n", args[1] );
            }//FIN ELSEIF
            else {
                chdir(args[0]);
                printf( "Change to directory %s\n", args[1] );
            }//FIN IF
            status_res = analyze_status(status, &info);
            if (info != 1) {
                printf("Foreground pid: %i, command: %s, %s, info: %i \n" ,getpid(), args[0], status_strings[status_res], info);
            }//FIN IF
            continue;
        }//FIN IF 
// -----------------------------------------------------------------------
//                            JOBS        
// -----------------------------------------------------------------------

        if (!strcmp(args[0], "jobs")) { //generamos el comando interno jobs
            if (list_size(tareas) == 0) {
                printf("\e[91mJobs list empty \e[0m\n" );
            }//FIN IF
            else {
                print_job_list(tareas);
            }//FIN ELSE 
            continue;
        }//FIN IF 

// -----------------------------------------------------------------------
//                            FG       
// -----------------------------------------------------------------------

        if (strcmp(args[0], "fg") == 0) { //es el comando interno foreground (fg)
            int posicion;
            enum status statusfg;
            job * aux;
            if (args[1] == NULL) { //Si args[1] no existe, cogemos la pos 1
                posicion = 1;
            }//FIN IF 
            else { //sino, la pos que le especifiquemos
                posicion = atoi(args[1]);
            }//FIN ELSE
            aux = get_item_bypos(tareas, posicion);
            if (aux == NULL) {
                printf("Error job not found \n");
                continue;
            }//FIN IF 
            if (aux->state == STOPPED  || aux->state == BACKGROUND) { 
                printf("Passed from background or suspended to foregroud the job: %s\n", aux->command);
                aux->state = FOREGROUND; //cambiamos el estado de auxiliar a foreground
                set_terminal(aux->pgid); //le damos el terminal
                killpg(aux->pgid, SIGCONT); //manda una señal al grupo de proceso para que continue
                waitpid(aux->pgid, &status, WUNTRACED); //esperamos por el hijo 
                set_terminal(getpid()); //el padre recupera el el terninal
                statusfg = analyze_status(status, &info);
                if (statusfg == SUSPENDED) {//si esta suspendido lo paramos 
                    aux->state = STOPPED;
                }//FIN IF 
                else {
                    delete_job(tareas, aux); //borramos el trabajo 
                }//FIN ELSE 
            }//FIN IF 
            else {
                printf("The process wasnt in background or suspended    ");
            }//FIN IF
            continue;
        }//FIN ELSE
// -----------------------------------------------------------------------
//                            BG       
// -----------------------------------------------------------------------

        if (strcmp(args[0], "bg") == 0) { //si es el comando interno backgraund (bg)
            int posicion;
            job * aux;
            if (args[1] == NULL) { //Si args[1] no existe, cogemos la pos 1
                posicion = 1;
            }// FIN IF 
            else {
                posicion = atoi(args[1]); //sino, la pos que le especifiquemos
            }//FIN ELSE
            aux = get_item_bypos(tareas, posicion); //selecionamos el trabajo en la posicion selecionada
            if (aux == NULL) {
                printf("Error job not found\n");
            } else if (aux->state == STOPPED) {// si esta parado lo pasamos a punto           
                aux->state = BACKGROUND;
                printf("passed to backgroun the process: %d that was suspended,  %s\n", aux->command);
                killpg(aux->pgid, SIGCONT);
            }// FIN ELSE IF 
            continue;
        }// FIN IF

        pid_fork = fork(); // creamos un proceso hijo guardando su pid en pid_fork
// -----------------------------------------------------------------------
//                            FORK ERRONEO         
// -----------------------------------------------------------------------

        if (pid_fork < 0) { // si pid es negativo es que no se ha creado el hijo
            perror( "\e[91mError en fork.\e[0m\n" ); // lanzamos un error y terminamos con -1
            exit(-1);
            continue;
        }// FIN IF

// -----------------------------------------------------------------------
//                            HIJO       
// -----------------------------------------------------------------------

        if (pid_fork == 0) { //acciones del hijo    
            new_process_group(getpid()); //cambia el grupo de grupo del proceso
            restore_terminal_signals(); //activamos las señales para el nuevo grupo
            execvp(args[0], args); //ejecutamos el nuevo comando (recordar que el codigo se cambia por el  codigo de la orden guardada en args)
            printf( "\e[91mError, command not found: %s \n\e[0m\n" , args[0]);
            exit(-1); // estatus si da fallo
        }//FIN IF

// -----------------------------------------------------------------------
//                            PADRE       
// -----------------------------------------------------------------------
        else { // acciones del padre
            new_process_group(pid_fork); //cambia el grupo de grupo del proceso hijo
            if (background == 0) { // si no esta en segundo plano
                set_terminal(pid_fork); //pedimos el terminal al hijo
                pid_wait = waitpid(pid_fork, &status, WUNTRACED); // espera que acabe el hijo
                set_terminal(getpid()); // debolvemos el terminal al padre 
                status_res = analyze_status(status, &info);
                if (status_res == SUSPENDED) { //si esta suspendido lo añadimos a la la lista de tareas 
                    job *item = new_job(pid_fork, args[0], STOPPED);
                    block_SIGCHLD(); //bloqueamos las señales de los hijos
                    add_job(tareas, item); // añadimos el trabajo a la lista
                    unblock_SIGCHLD(); //desbloqueamos loas señales
                  }//FIN IF 
                if (info != 1) {// si no se ha parado  
                     
                    printf("\e[1;36mForeground pid: %d, command: %s, %s, info: %d\n \n\e[0m",pid_fork, args[0], status_strings[status_res], info);
                }//FIN IF
            }//FIN IF 
            else { // si esta en segundo plano
                job * item = new_job(pid_fork, args[0], BACKGROUND); //3
                block_SIGCHLD(); //bloqueamos las señales de los hijos
                add_job(tareas, item); // añadimos el trabajo a la lista
                unblock_SIGCHLD(); //desbloqueamos loas señales
                if (info != 1) {//si no se ha parado
                    printf("\e[1;33mBackground job running... pid: %i, command: %s \n\n\e[0m" , pid_fork, args[0]);
            }// FIN IF
           }//FIN ELSE   
        }//FIN ELSE
    } // end while
}//FIN MAIN