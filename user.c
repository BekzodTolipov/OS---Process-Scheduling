/********************************************************************
*	Program designed as executable by forked child by oss.c and		*
*	reads oss.c's simulated time and generates random number		*
*	between 1 to 1,000,000 and adds it to it's local timer.			*
*	After reaching its maximum time to be alive it finally			*
*	sends message to oss.c and kills itself							*
*********************************************************************/

#include <stdio.h> 
#include <sys/ipc.h> 
#include <sys/shm.h> 
#include <math.h>
#include <string.h>
#include <fcntl.h>
#include <sys/sem.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdlib.h>
#include <stdbool.h>
#include <signal.h>
#include <string.h>
#include <stdarg.h> 
#include <sys/types.h>
#include <time.h>
#include "shared_mem.h"
//Prototypes
void fix_time();
static int setupinterrupt();
static void myhandler(int s);
void strfcat(char *fmt, ...);
void sem_clock_lock();
void sem_clock_release();
void sem_print_lock();
void sem_print_release();


int rand_numb;
static bool quit;
int sec;
int ns;
int *check_permission;
struct Clock *clock_point;
char buffer[2048];
/* structure for semaphore operations */
struct sembuf sem_op;
int sem_id;
pid_t child_id;
pid_t parent_id;
unsigned int start_time;

/****************
* Main function *
****************/
int main(int argc, char *argv[]) 
{
	int shmid;
	child_id = getpid();
	parent_id = getppid();
	quit = false;
	srand((unsigned) time(NULL));
	rand_numb = (rand() % (5 - 1000000 + 1)) + 5;	
	
	key_t key = ftok("./oss.c", 21);
	
	shmid = shmget(key, sizeof(struct Clock), IPC_CREAT | 0644);

	// ftok to generate unique key 
    key_t key_2 = ftok("./oss.c", 22); 
	key_t key_3 = ftok("./oss.c", 23);
	key_t key_4 = ftok("./oss.c", 24);
	
    // shmget returns an identifier in shmid 
    int shmid_2 = shmget(key_2,2048,0666 | IPC_CREAT); 
	int shmid_3 = shmget(key_4, sizeof(int), 0666 | IPC_CREAT);
  
    // shmat to attach to shared memory 
    shmMsg = (char*) shmat(shmid_2, NULL, 0);
	check_permission = (int*) shmat(shmid_3, NULL, 0); 

	if(setupinterrupt() == -1){
         fprintf(stderr, "ERROR: Failed to set up handler");
         return 1;
    }

	sem_id = semget(key_3, 2, IPC_CREAT | IPC_EXCL | 0666);
	semctl(sem_id, 0, SETVAL, 1);
	semctl(sem_id, 1, SETVAL, 1);
	clock_point = shmat(shmid, NULL, 0);
	
	//Set up child max duration
	sem_clock_lock();		//Lock Sem simulated clock
	sec = clock_point->sec;	//Copy sec to local
	ns = clock_point->ns;	//Copy ns to local
	sem_clock_release;		//Release Sem
	ns += rand_numb;

	fix_time();

    int i;
    while (!quit) {
		sem_clock_lock();	//Lock Sem simulated clock
		printf("\n!Enter clock check! PID: %d\n", getpid());
		if(*check_permission == 0){
			printf("\n!!Passed permission check!! PID: %d\n", getpid());
			if(clock_point->sec == sec){
				if ( clock_point->ns > ns){
					sem_print_lock();	//Lock Sem shared MESSAGE memory
					strfcat("child reached %d.%d in child process\n", sec, ns);
					*check_permission = 1;	//Tell oss that MESSAGE ready	
					sem_print_release();	//Release Sem shared MESSAGE memory
					quit = true;	//Stop looping
				}
			}
			else if(clock_point->sec > sec){
				sem_print_lock();	//Lock Sem shared MESSAGE memory

				strfcat("child reached %d.%d in child process\n", sec, ns);
				*check_permission = 1;	//Tell oss that MESSAGE ready 
				sem_print_release();	//Release Sem shared MESSAGE memory
				quit = true;	//Stop looping
			}
		}
		else{
			quit = false;
		}

		printf("\n!Left clock check! PID: %d\n", getpid());
		sem_clock_release();
		
	}
	//detach memory
	shmdt(shmMsg);
	shmdt(clock_point);

	return 0;
}
/*****************************************************************
* Function to increment seconds if nanoseconds reached 1 billion *
*****************************************************************/
void fix_time(){

	if(ns > 1000000000){
		sec++;
		ns -= 1000000000;
	}

}

/*******************
* Set up interrupt * 
*******************/
static int setupinterrupt(){
    struct sigaction act;
    act.sa_handler = &myhandler;
    act.sa_flags = SA_SIGINFO;
    return(sigemptyset(&act.sa_mask) || sigaction(SIGTERM, &act, NULL));
}

/************************
* Set up my own handler *
************************/
static void myhandler(int s){
	shmdt(shmMsg);
    shmdt(clock_point);

	exit(1);
}

/**************************************
* Copy child message to shared memory *
**************************************/
void strfcat(char *fmt, ...){
	va_list args;
	
	va_start(args, fmt);
	vsprintf(buffer, fmt, args);
	va_end(args);

	strcpy(shmMsg, buffer);
}

/***********************
* Lock clock semaphore *
***********************/
void sem_clock_lock(){
    sem_op.sem_num = 0;
    sem_op.sem_op = -1;
    sem_op.sem_flg = 0;
    semop(sem_id, &sem_op, 1);
}

/**************************
* Release clock semaphore *
**************************/
void sem_clock_release(){
    sem_op.sem_num = 0;
    sem_op.sem_op = 1;
    sem_op.sem_flg = 0;    
	semop(sem_id, &sem_op, 1);
}

/***********************
* Lock print semaphore *
***********************/
void sem_print_lock(){
    sem_op.sem_num = 1;
    sem_op.sem_op = -1;
    sem_op.sem_flg = 0;
    semop(sem_id, &sem_op, 1);
}

/**************************
* Release print semaphore *
**************************/
void sem_print_release(){
    sem_op.sem_num = 1;
    sem_op.sem_op = 1;
    sem_op.sem_flg = 0;
    semop(sem_id, &sem_op, 1);
}
