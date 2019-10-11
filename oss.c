/********************************************************************************************  
 *		Program Description: Simple program demonstration of inter process communication	*
 *      by using following system calls(fork, exec, shared memory, and semaphore)			*
 *      "oss" will generate shared memory for simulated clock which will have seconds		*
 *		and nanoseconds, in addition to that shared for messages.							*
 *		"oss" will start by forking of by given maximum number of process allowed to be		*
 *		spawned and keep itterating loop and increment simulated clock by 10,000.			*
 *		In addition to that it will replace all kids thats been terminated along the		*
 *		way.																				*
 *		After each termination of children oss will receive message and output that			*
 *		message along with it's own message to give or default output file					*
 *																							*
 *      Author: Bekzod Tolipov																*
 *      Date: 10/01/2019																	*
 *      Class: CS:4760-001																	*
 *******************************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <sys/types.h>
#include <string.h>
#include <math.h>
#include <sys/ipc.h> 
#include <sys/shm.h> 
#include <unistd.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/sem.h>
#include <fcntl.h>          /* O_CREAT, O_EXEC */
#include <stdbool.h>
#include <sys/wait.h>
#include "shared_mem.h"

//Prototypes
que* queue_generator(int numb_kids);
int random_numb_gen(int min, int max);
us_p _init_user_process(int index, pid_t child_id);
pcb copy_user_to_pcb(pcb pcb, us_p user_process, int id);
void push_to_queue(struct queue* queue, pcb pcb);
void fix_time();
void sem_clock_lock();
void sem_clock_release();
//queue_lvl_find(int);



////////////////

pcb* pcb;
simulated_clock* sim_clock;
que* low_queue;
que* med_queue;
que* high_queue;
int msg_queue_id;
int *check_permission;
int clock_point;
int pcb_shmid;
pid_t parent_pid;
pid_t child_id;
pid_t child_arr[18];
static bool quit;
char *shmMsg;


#define MAXCHAR 1024
int main(int argc, char **argv){

	char file_name[MAXCHAR] = "log.dat";
	char dummy[MAXCHAR];
	int numb_child = 5;
	int max_time = 5;
	int c;
	quit = false;
	
	parent_pid = getpid();
	// Read the arguments given in terminal
	while ((c = getopt (argc, argv, "hs:l:t:")) != -1){
		switch (c)
		{
			case 'h':
				printf("To run the program you have following options:\n\n[ -h for help]\n[ -s maximum number of processes spawned ]\n[ -l filename ]\n[ -t time in seconds ]\nTo execute the file follow the code:\n./%s [ -h ] or any other options", argv[0]);
				return 0;
			case 's':
				strncpy(dummy, optarg, 255);
                numb_child = toint(dummy);
                if(numb_child > 20 || numb_child < 1){
					fprintf(stderr, "ERROR: number of children should between 1 and 20");
					abort();
				}
				break;
			case 'l':
				strncpy(file_name, optarg, 255);
				break;
			case 't':
				strncpy(dummy, optarg, 255);
				max_time = toint(dummy);
                break;
			default:
				fprintf(stderr, "ERROR: Wrong Input is Given!");
				abort();

		}
	}

	FILE *fptr;
	fptr = fopen(file_name, "w");
	// Validate if file opened correctly
	if(fptr == NULL){
		fprintf(stderr, "Failed to open the file, terminating program\n");
		return 1;
	}
	setvbuf(fptr, NULL, _IONBF, 0);

	// ftok to generate unique key 
	key_t msg_queue_key = ftok("./oss.c", 21);  
	key_t clock_key = ftok("./oss.c", 22);
	key_t pcb_key = ftok("./oss.c", 23);
	key_t sem_key = ftok("./oss.c", 24);

	// System Timer set up
    if(setuptimer(max_time) == -1){
        fprintf(stderr, "ERROR: Failed set up timer");
        fclose(fptr);
		return 1;
    }
	// System Interrupt set up
    if(setupinterrupt() == -1){
         fprintf(stderr, "ERROR: Failed to set up handler");
         fclose(fptr);
         return 1;
    }

	// We initialize the semaphore id
	sem_id = semget(sem_key, 2, IPC_CREAT | IPC_EXCL | 0666);
	if(sem_id == -1){
		fprintf(stderr, "ERROR: Failed semget");
		return 1;
	}
	// Now we will set up 2 semaphores
	semctl(sem_id, 0, SETVAL, 1);
	semctl(sem_id, 1, SETVAL, 1);

	int i;
    for(i=0; i<18; i++){	//Initialize array to zero's
			child_arr[i] = 0;
	}

	msg_queue_id = msgget(msg_queue_key, IPC_CREAT | 0644 );

	if ((clock_shmid = shmget(clock_key, sizeof(simulated_clock), IPC_CREAT | 0644)) < 0) {
        fprintf(stderr, "shmat failed on shared message");
		return 1;
    }

    clock_point = shmat(clock_shmid, NULL, 0);
	
	//Set up starter values for clock and message signal
	*check_permission = 0;
	clock_point->sec = 0;
	clock_point->ns = 0;

	//Set up pcb starter
    if ((pcb_shmid = shmget(pcb_key, sizeof(struct ProcessControlBlock) * 18, IPC_CREAT | 0644)) < 0) {
        fprintf(stderr, "shmat failed on shared message");
		return 1;
    }
	//Attach shared mem to pcb
    pcb = shmat(pcb_shmid, NULL, 0);

	//Set up queues high, medium, low
	high_queue = queue_generator(18);
	med_queue = queue_generator(18);
	low_queue = queue_generator(18);

	int counter = 0;
	int spawn = 0;
	int prev_clock = 0;
	int queue_lvl = -1;
	for(i = 0; i < 18; i++) {
        if(counter <= 18) {
            spawning = random_numb_gen(0, 2);
            if((clock_point->sec - prev_clock) >= spawning) {
                child_id = fork();

                if(child_id == 0) {
                    us_p user_process = _init_user_process(i, getpid());

                    pcb[i] = copy_user_to_pcb(pcb[i], userProcess, i);

				//	queue_lvl = queue_lvl_find(pcb[i].priority);

                    fprintf(stderr, "\nOSS: Generating process with PID %d and putting it in queue %d at time %d.%d\n", pcb[i].actualPid, pcb[i].priority, clock_point->sec, clock_point->ns);

                    if(pcb[i].priority == 0) {
						if(high_queue->size < high_queue->max_kids){
							push_to_queue(high_queue, pcb[i]);
						} 
						else 
						{
							fprintf(stderr, "OSS: HIGH QUEUE IS FULL");
						}
					} 
					else if(pcb.priority[i] == 1) {
						if(med_queue->size < med_queue->max_kids){
							pushToQueue(low_queue, pcb[i]);
						} 
						else 
						{
							fprintf(stderr, "OSS: MEDIUM QUEUE IS FULL");
						}
					}
					else{
						if(low_queue->size < low_queue->max_kids){
							pushToQueue(low_queue, pcb[i]);
						} 
						else 
						{
							fprintf(stderr, "OSS: LOW QUEUE IS FULL");
						}
					}
					//push_to_queue_or_not(pcb[i]);


                } 
				else if (childPid < 0) {
                    perror("[-]ERROR: Failed to fork CHILD process.\n");
                    exit(errno);
                }
            }

            counter++;

            prev_clock = clock_point->sec;

            fprintf(stderr, "OSS: Dispatching process with PID %d from queue %d at time %d:%d", pcb[i].process_id, pcb[i].priority, clock_point->sec, clock_point->ns);

            //advanceSharedMemoryClock();
			sem_clock_lock();

			//increment seconds
			clock_point->ns += 1000;
			fix_time();
			// Release the critical section
			sem_clock_release();
		}
    }


	return 0;
}

struct que* queue_generator(int numb_kids){
	
	struct que* queue = (struct que*) malloc(sizeof(struct que));

    queue->max_kids = numb_kids;
    queue->beginning = 0;
	queue->size = 0;
    queue->end = numb_kids - 1;
    queue->array = (pcb*)malloc(que->max_kids * sizeof(pcb));
   
	return queue;

}

int random_numb_gen(int min, int max) {

    return rand()%(max - min) + min;

}

us_p _init_user_process(int index, pid_t child_id){

	us_p* user_process = (struct user_process*) malloc(sizeof(struct user_process));
	user_process->id = index;
	user_process->process_id = child_id;
	user_process->priority = 0;

	int randNum = randomNumberGenerator(3, 0);
   if(randNum == 0) {
       user_process->duration = 0;
       user_process->waitTime = 0;

   } else if(randNum == 1) {
       user_process->duration = 50000;
       user_process->waitTime = 0;

   } else if(randNum == 2) {
       int seconds = random_numb_gen(0, 4);
       int millis = random_numb_gen(0, 999) * 1000000;
       int total_nanos = seconds * 1000000000 + millis;
		
      // int totalNanos = 0;
      // if((millis % 1000) == 0) {
      //     totalNanos = millis * 100000000;

      // } else if((millis % 100) == 0) {
      //     totalNanos = millis * 10000000;

      // } else if((millis % 10) == 0) {
      //     totalNanos = millis * 1000000;

     //  } else if((millis % 1) == 0) {
      //     totalNanos = millis * 100000;
     //  }

       user_process->waitTime = millis + total_nanos;
       user_process->duration = 50000;

   } else if(randNum == 3) {
       double p_value = random_numb_gen(1, 99) / 100;
       user_process->duration = user_process->burstTime - (p_value * user_process->burstTime);
       user_process->waitTime = 0;
   }

    return *user_process;
}

pcb copy_user_to_pcb(pcb pcb, us_p user_process, int id){

	pcb.id = id;
    pcb.process_id = user_process.process_id;
	pcb.priority = user_process.priority;
	pcb.burstTime = user_process.burstTime;
    pcb.duration = user_process.duration;	
	pcb.waitTime = user_process.waitTime;

    return pcb;
}

void push_to_queue(struct queue* queue, pcb pcb) {
    if(queue->size < queue->max_kids) {
        queue->end = (queue->end++)%queue->max_kids;
        queue->array[queue->end] = pcb;
        queue->size++;
    }
}

/***********************************************
* Function to trim down nanoseconds to seconds *
***********************************************/
void fix_time(){
    if((int)(clock_point->ns / 1000000000) == 1){
        clock_point->sec++;
        clock_point->ns -= 1000000000;
    }
}

/***************************
* Lock the clock semaphore *
***************************/
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

