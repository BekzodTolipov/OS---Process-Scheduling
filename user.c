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
#include<sys/msg.h>
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
int convert_to_millis(int q);
long int convert_to_ns(int q);
int random_numb_gen(int min, int max);



int rand_numb;
static bool quit;
int sec;
int ns;
unsigned int start_time;
int total_quantum_in_millis;
int *check_permission;
//struct Clock *clock_point;
char buffer[2048];
/* structure for semaphore operations */
struct sembuf sem_op;
int sem_id;
pid_t child_id;
pid_t parent_id;
//unsigned int start_time;
int msg_queue_id;
simulated_clock *clock_point;
US_P *user_process;
int quantum;
int pcb_shmid;
PCB *pcb;
static struct message msg;
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
	int burst_time;
	int pcb_id = atoi(argv[1]);

	if(setupinterrupt() == -1){
         fprintf(stderr, "ERROR: Failed to set up handler");
         return 1;
    }

	srand((unsigned) time(NULL));
	rand_numb = (rand() % (5 - 1000000 + 1)) + 5;	
	
	key_t clock_key = ftok("./oss.c", 22);
	
	if ((shmid = shmget(clock_key, sizeof(struct Clock), IPC_CREAT | 0644)) < 0){
		fprintf(stderr, "ERROR: USER: Failed access to shared clock");
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
	//int shmid_3 = shmget(key_4, sizeof(int), 0666 | IPC_CREAT);
	//Set up pcb starter
    if ((pcb_shmid = shmget(pcb_key, sizeof(struct process_control_block) * 3, IPC_CREAT | 0644)) < 0) {
        fprintf(stderr, "EEROR: USER: shmat failed on shared pcb");
		return 1;
    }
	//Attach shared mem to pcb
    pcb = shmat(pcb_shmid, NULL, 0);
  
    // shmat to attach to shared memory 
    //shmMsg = (char*) shmat(shmid_2, NULL, 0);
	//check_permission = (int*) shmat(shmid_3, NULL, 0); 

	if(setupinterrupt() == -1){
         fprintf(stderr, "ERROR: Failed to set up handler");
         return 1;
    }

	sem_id = semget(sem_key, 2, IPC_CREAT | IPC_EXCL | 0666);
	semctl(sem_id, 0, SETVAL, 1);
	semctl(sem_id, 1, SETVAL, 1);
	clock_point = shmat(shmid, NULL, 0);

	//fprintf(stderr, "USER: getting in to while loop\n");
	// User process
	//user_process = (struct user_process*) malloc(sizeof(struct user_process));
	//msg = (struct message*) malloc(sizeof(struct message));
	
	while (!quit) {
		//Set up child max duration
	//	sem_clock_lock();       //Lock Sem simulated clock
		sec = clock_point->sec; //Copy sec to local
		ns = clock_point->ns;   //Copy ns to local
	//	sem_clock_release();      //Release Sem
		start_time = convert_to_ns(sec) + ns;
		pcb[pcb_id].terminate = false;
		while(1){
			
			//fprintf(stderr, "USER: getting in to while loop: pid(%d)\n", getpid());
			int result = msgrcv(msg_queue_id, &msg, (sizeof(Message) - sizeof(long)), getpid(    ), IPC_NOWAIT);
			if(result != -1){
				fprintf(stderr, "USER: recieved msg pid: %d", msg.process_id);
				if(msg.process_id == getpid()){
					//fprintf(stderr, "USER: Quantum decisions\n");
					quantum = rand()%2 == 0? QUANTUM : QUANTUM/2;
					
					int random_number = random_numb_gen(0, 4);
				//	int burst_time;
					int burst_rand = rand()%2 == 1? 1 : 0;
					if(burst_rand == 1){
						burst_time = quantum;
					}
					else{
						burst_time = random_numb_gen(0, quantum);
					}
					
					pcb[pcb_id].burst_time = burst_time;

					int random_num = rand() % 4 + 0;
					//fprintf(stderr, "\nUSER: Quantum decisions: %d\n", random_num);
					
					if(random_num == 0) {
					   pcb[pcb_id].duration = 0;
					   pcb[pcb_id].wait_time = 0;
					   pcb[pcb_id].terminate = true;

					} else if(random_num == 1) {
					   pcb[pcb_id].duration = burst_time;
					   pcb[pcb_id].wait_time = 0;
					   //pcb[pcb_id].terminate = false;

					} else if(random_num == 2) {
					   int seconds = random_numb_gen(0, 5);
					//	fprintf(stderr, "PCB duration: %d\n", pcb[getpid()].id);
					   int milli_sec = random_numb_gen(0, 1000) * 1000000;
					   int total_nanos = seconds * 1000000000 + milli_sec;
						

						//fprintf(stderr, "PCB duration: %d\n", pcb[getpid()].id);
					   pcb[pcb_id].wait_time = milli_sec + total_nanos;
					   pcb[pcb_id].duration = burst_time;
					  // pcb[pcb_id].terminate = false;

					} else if(random_number == 3) {
					   double p_value = random_numb_gen(1, 99) / 100;
					   pcb[pcb_id].duration = p_value * burst_time;
					   pcb[pcb_id].wait_time = 0;
					 //  pcb[pcb_id].terminate = false;
					}	
					
					msg.mtype = 1;
					msg.process_id = child_id;
					msg.id = pcb_id;
					msg.sec = clock_point->sec;
					msg.ns = clock_point->ns;
					//fprintf(stderr, "USER: Sending message back\n");
					msgsnd(msg_queue_id, &msg, (sizeof(Message) - sizeof(long)), 0);
					break;
				}
			}
		}
		
		unsigned int beginning = convert_to_ns(clock_point->sec) + clock_point->ns;
		while(1)
		{
			pcb[pcb_id].duration -= convert_to_ns(clock_point->sec) + clock_point->ns - beginning;

			if(pcb[pcb_id].duration <= 0)
			{
				//Send a message to master how long I ran for
				msg.mtype = 1;
				msg.id = pcb_id;
				msg.process_id = child_id;
				msg.ns = pcb[pcb_id].burst_time;
				//fprintf(stderr, "\n!!!USER: Sending message back after duration\n");
				msgsnd(msg_queue_id, &msg, (sizeof(Message) - sizeof(long)), 0);
				break;
			}
		}
		
		if(pcb[pcb_id].terminate){
			msg.done_flag = 0;
		}
		else{
			msg.done_flag = 1;
		}
		
		msg.mtype = 1;
		msg.process_id = child_id;
		msg.id = pcb_id;
		msg.burst_time = pcb[pcb_id].burst_time;
		msg.duration = pcb[pcb_id].duration;
		msg.wait_time = pcb[pcb_id].wait_time + convert_to_ns(clock_point->sec) + clock_point->ns - beginning;
		
		msgsnd(msg_queue_id, &msg, (sizeof(Message) - sizeof(long)), 0);

		msg.process_id = -1;
		if(pcb[pcb_id].terminate){
			fprintf(stderr, "\n\nUSER: termination\n");
			break;
		}
	}
	fprintf(stderr, "USER: exiting\n");
	shmdt(clock_point);
	shmdt(pcb);
//	free(user_process);
	//free(msg);

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
    int progress;
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
	//shmdt(shmMsg);
    shmdt(clock_point);
	shmdt(pcb);

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

//	strcpy(shmMsg, buffer);
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

int convert_to_millis(int q){

	return q / 1000000;

}

long int convert_to_ns(int q){

	return q * 1000000000;

}

