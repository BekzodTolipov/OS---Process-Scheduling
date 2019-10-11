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
int start_time;
int total_quantum_in_millis;
int *check_permission;
//struct Clock *clock_point;
char buffer[2048];
/* structure for semaphore operations */
struct sembuf sem_op;
int sem_id;
pid_t child_id;
pid_t parent_id;
unsigned int start_time;
int msg_queue_id;
simulated_clock *clock_point;
us_p *user_process;
int quantum;
int pcb_shmid;
pcb *pcb;
//simulated_clock *start_time;

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
	
	key_t clock_key = ftok("./oss.c", 22);
	
	if ((shmid = shmget(clock_key, sizeof(struct Clock), IPC_CREAT | 0644)) < 0){
		fprintf(stderr, "ERROR: Failed access to shared clock");
		return 1;
	}

	// ftok to generate unique key 
    key_t msg_queue_key = ftok("./oss.c", 21); 
	key_t pcb_key = ftok("./oss.c", 23);
	key_t sem_key = ftok("./oss.c", 24);
	
	// msgget return id
	msg_queue_id = msgget(msg_queue_key, 0666);
    // shmget returns an identifier in shmid 
    //int shmid_2 = shmget(key_2,2048,0666 | IPC_CREAT); 
	int shmid_3 = shmget(key_4, sizeof(int), 0666 | IPC_CREAT);
	//Set up pcb starter
    if ((pcb_shmid = shmget(pcb_key, sizeof(struct ProcessControlBlock) * 18, IPC_CREAT | 0644)) < 0) {
        fprintf(stderr, "shmat failed on shared message");
		return 1;
    }
	//Attach shared mem to pcb
    pcb = shmat(pcb_shmid, NULL, 0);
  
    // shmat to attach to shared memory 
    //shmMsg = (char*) shmat(shmid_2, NULL, 0);
	check_permission = (int*) shmat(shmid_3, NULL, 0); 

	if(setupinterrupt() == -1){
         fprintf(stderr, "ERROR: Failed to set up handler");
         return 1;
    }

	sem_id = semget(sem_key, 2, IPC_CREAT | IPC_EXCL | 0666);
	semctl(sem_id, 0, SETVAL, 1);
	semctl(sem_id, 1, SETVAL, 1);
	clock_point = shmat(shmid, NULL, 0);

	// User process
	user_process = (struct user_process*) malloc(sizeof(struct user_process));
	
	//Set up child max duration
	sem_clock_lock();		//Lock Sem simulated clock
	sec = clock_point->sec;	//Copy sec to local
	ns = clock_point->ns;	//Copy ns to local
	sem_clock_release;		//Release Sem
	start_time = sec * 1000000000 + ns;
	// Decide if full quantum or half
//	rand()%2 == 0? quantum = user_process->duration : quantum = random_numb_gen(0, user_process->duration/2);
//	total_quantum_in_millis = convert_to_millis(quantum);
/*	it updates its process control block by adding to the accumulated CPU time. It joins the ready queue at that
point and sends a signal on a semaphore so that oss can schedule another process.?????????????????? */


	//ns += rand_numb;

	//fix_time();

    int i;
    while (!quit) {
		//Set up child max duration
		sem_clock_lock();       //Lock Sem simulated clock
		sec = clock_point->sec; //Copy sec to local
		ns = clock_point->ns;   //Copy ns to local
		sem_clock_release;      //Release Sem
		start_time = sec * 1000000000 + ns;

		// Decide if full quantum or half
		rand()%2 == 0? quantum = user_process->duration : quantum = random_numb_gen(0, user_process->duration/2);
		total_quantum_in_millis = convert_to_millis(quantum);
 /*  it updates its process control block by adding to the accumulated CPU time. It joins the ready queue at that
125 point and sends a signal on a semaphore so that oss can schedule another process.?????????????????? */
		if(total_quantom_in_millis < "50millis"){
			if(pcb[child_id].is_scheduled == 1){
				sem_print_lock();
				while("quantum+start_time in nanoseconds" < convert_to_ns(clock_point->sec) + clock_point->ns);
				message msg;
				msg->process_id = child_id;
				msg->is_done = 1;
				msg->sec = clock_point->sec;
				msg->ns = clock_point->ns;
				msgsnd(msg_queue_id, &msg, sizeof(message)-sizeof(long), 0);
				sem_print_release();
			}
		}
		else{
			int decide = -1;
			rand()%2 == 1? decided = 1 : decided = 0;
			if(decided){
				quit = false;
			}
			else{
				total_quantom_in_millis -= "50 millis";
			}
		}
	}
	//detach memory
	//shmdt(shmMsg);
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

int random_numb_gen(int min, int max) {

    return rand()%(max - min) + min;

}
