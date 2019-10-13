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
#include <sys/msg.h>
#include <errno.h>
#include "shared_mem.h"

//Prototypes
QUE* queue_generator(int numb_kids);
int random_numb_gen(int min, int max);
US_P _init_user_process(int index, pid_t child_id);
PCB copy_user_to_pcb(PCB pcb, US_P user_process, int id);
void push_to_queue(struct queue* queue, PCB pcb);
void pop_from_queue(QUE* queue);
void fix_time();
void sem_clock_lock();
void sem_clock_release();
int convert_to_millis(int q);
long int convert_to_ns(int q);
static int setuptimer(int s);
static int setupinterrupt();
static void myhandler(int s);
void fix_time();
//int toint(char str[]);


//queue_lvl_find(int);



////////////////

PCB* pcb;
simulated_clock* sim_clock;
QUE* low_queue;
QUE* med_queue;
QUE* high_queue;
int msg_queue_id;
int *check_permission;
simulated_clock* clock_point;
int pcb_shmid;
pid_t parent_pid;
pid_t child_id;
int bit_vector[18];
static bool quit;
int sem_id;
//Semaphore set up
struct sembuf sem_op;
int clock_shmid;
long int total_ns;
Message msg;
//char *shmMsg;


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
                //numb_child = toint(dummy);
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
				//max_time = toint(dummy);
                break;
			default:
				fprintf(stderr, "ERROR: Wrong Input is Given!");
				abort();

		}
		fprintf(stderr, "PCB duration: %d\n", pcb[getpid()].id);
	}

	FILE *fptr;
	fptr = fopen(file_name, "w");
	// Validate if file opened correctly
	if(fptr == NULL){
		fprintf(stderr, "ERROR: Failed to open the file, terminating program\n");
		return 1;
	}
	setvbuf(fptr, NULL, _IONBF, 0);

	// ftok to generate unique key 
	key_t msg_queue_key = ftok("./oss.c", 21);  
	key_t clock_key = ftok("./oss.c", 22);
	key_t pcb_key = ftok("./oss.c", 23);
	key_t sem_key = ftok("./oss.c", 24);

	// System Timer set up
    if(setuptimer(10) == -1){
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
			bit_vector[i] = 0;
	}

	msg_queue_id = msgget(msg_queue_key, IPC_CREAT | 0666);

	if ((clock_shmid = shmget(clock_key, sizeof(simulated_clock), IPC_CREAT | 0600)) < 0) {
        fprintf(stderr, "ERROR: shmat failed on shared message");
		return 1;
    }

    clock_point = shmat(clock_shmid, NULL, 0);
	
	//Set up starter values for clock and message signal
	//*check_permission = 0;
	clock_point->sec = 0;
	clock_point->ns = 0;

	//Set up pcb starter
    if ((pcb_shmid = shmget(pcb_key, sizeof(struct process_control_block) * 18, IPC_CREAT | 0666)) < 0) {
        fprintf(stderr, "shmat failed on shared message");
		return 1;
    }
	//Attach shared mem to pcb
    pcb = shmat(pcb_shmid, NULL, 0);

	//Set up queues high, medium, low
	high_queue = queue_generator(18);
	med_queue = queue_generator(18);
	low_queue = queue_generator(18);

	//int counter = 0;
	int spawning = 0;
	int prev_clock = 0;
	int queue_lvl = -1;
	i = 0;
	int start_ns = 0;
	int which_queue = 0;
	int total_cpu_spent_user = 0;
	int total_cpu_spent_queue = 0;
	long int start_time = 0;
	char index_str[MAXCHAR];
	while(!quit) {
		if((i%18) == 0){
			i = 0;
		}
		start_time = convert_to_ns(clock_point->sec) + clock_point->ns;
		//fprintf(stderr, "BBBITTT vector before creating: %d\n\n", bit_vector[i]);
        if(bit_vector[0] == 0) {
            spawning = random_numb_gen(0, 2);
            if((clock_point->sec - prev_clock) >= spawning) {
                child_id = fork();
				bit_vector[i] = 1;
				//fprintf(stderr, "BBBITTTT vector after creating %d\n\n", bit_vector[i]);
                if(child_id == 0) {
                    US_P user_process = _init_user_process(i, getpid());

                    pcb[i] = copy_user_to_pcb(pcb[i], user_process, i);


                    fprintf(stderr, "\nOSS: Generating process with PID %d and putting it in queue %d at time %d.%d\n", pcb[i].process_id, pcb[i].priority, clock_point->sec, clock_point->ns);

                    //if(pcb[i].priority == 0) {
					//	if(high_queue->size < high_queue->max_kids){
		//	void push_to_queue(struct queue* queue, pcb pcb) {

					push_to_queue(high_queue, pcb[i]);
					pcb[i].is_scheduled = 1;
					//index_str = toint(i);
					sprintf(index_str, "%d", i);
					execl("./child", "./child", index_str, NULL);
					//	} 
					//	else 
					//	{
					//		fprintf(stderr, "OSS: HIGH QUEUE IS FULL");
					//	}
				//	} 
				//	else if(pcb.priority[i] == 1) {
				//		if(med_queue->size < med_queue->max_kids){
				//			push_to_queue(low_queue, pcb[i]);
				//		} 
				//		else 
				//		{
				//			fprintf(stderr, "OSS: MEDIUM QUEUE IS FULL");
				//		}
				//	}
				//	else{
				//		if(low_queue->size < low_queue->max_kids){
				//			push_to_queue(low_queue, pcb[i]);
				//		} 
				//		else 
				//		{
				//			fprintf(stderr, "OSS: LOW QUEUE IS FULL");
				//		}
				//	}
					//push_to_queue_or_not(pcb[i]);


                } 
				else if (child_id < 0) {
                    perror("ERROR: Fork() failed\n");
                    return 1;
                }
            }

			int stat;
			pid_t remove_pid = waitpid(-1, &stat, WNOHANG);	// Non block wait for parent
		fprintf(stderr, "PCB duration: %d\n", pcb[getpid()].id);
		// If somebody died then barry them underground
		// and remove them from history
			if(remove_pid > 0){
				fprintf(stderr, "\n\nOSS: deleting child from array\n\n");
				int pos;
				for(pos=0; pos<18;pos++){
					if(pcb[pos].process_id == remove_pid){
						bit_vector[pcb[pos].id] = 0;
					}
				}
			}

            //counter++;

    //int progress;
			//sem_clock_lock;
            prev_clock = clock_point->sec;
			//sem_clock_release;

			total_ns = clock_point->ns - start_ns;
			//sem_clock_release;
            fprintf(stderr, "OSS: total time this dispatch was %li nanoseconds\n", total_ns);
    //int progress;
			//msg msg;
			static int messageSize = sizeof(Message) - sizeof(long);
			/* receive a message */
			msgrcv(msg_queue_id, &msg, messageSize, 1, IPC_NOWAIT); // type is 0
			//	fprintf(stderr, "\nOSS: exit: msgrcv error, %s\n", strerror(errno)); // error
				//return 1;
			//}
			//if(msg.done_flag == 1){
			//	pcb[msg.id].is_scheduled = 0;
			//}
			if(errno == ENOMSG){
				//no message yet increment clock here
			//	errno = 0;
				fprintf(stderr, "OSS: NO MSG increment time!\n");
				sem_clock_lock();

				//increment seconds
				clock_point->ns += 1000;
				fix_time();
				// Release the critical section
				sem_clock_release();
				//increment clock
			}
			//msgrcv(msg_queue_id, &msg, messageSize, msg_queue_key, 0);
			else{
				fprintf(stderr, "OSS: received message!!\n");
				total_cpu_spent_queue -= start_time - msg.duration;
				if(which_queue == 0){
					total_cpu_spent_user += msg.duration;
					if(total_cpu_spent_user > thresh_hold_user){
						pop_from_queue(high_queue);
						pcb[msg.process_id].priority = 1;
						total_cpu_spent_user = 0;
					}
					if(total_cpu_spent_queue > thresh_hold_oss){
						which_queue = 1;
					}
				}
				else if(which_queue == 1){
					total_cpu_spent_user += msg.duration;
					if(total_cpu_spent_user > thresh_hold_user){
						pop_from_queue(med_queue);
						pcb[msg.process_id].priority = 2;
						total_cpu_spent_user = 0;
	//	fprintf(stderr, "PCB duration: %d\n", pcb[getpid()].id);
					}
					if(total_cpu_spent_queue > thresh_hold_oss){
						which_queue = 2;
					}

				}
				else{
					total_cpu_spent_user += msg.duration;
					if(total_cpu_spent_user > thresh_hold_user){
						pop_from_queue(low_queue);
						pcb[msg.process_id].priority = 0;
						total_cpu_spent_user = 0;
					}
					if(total_cpu_spent_queue > thresh_hold_oss){
						which_queue = 0;
					}

				}
				//advanceSharedMemoryClock();
				sem_clock_lock();

				//increment seconds
				clock_point->ns += 1000;
				fix_time();
				// Release the critical section
				sem_clock_release();
			}
			i++;
		}
		//else{
		//	fprintf(stderr, "\nStart to terminate everything current i: %d\n", i);
		//	for(i=0; i<20; i++){
		//		if(pcb[i].process_id != 0){
		//			if(kill(pcb[i].process_id, 0) == 0){
		//				if(kill(pcb[i].process_id, SIGTERM) != 0){
		//					perror("Child can't be terminated for unkown reason\n");
		//				}
		//			}
		//		}
		//	}

		//	for(i=0;i<20;i++){
		//		if(pcb[i].process_id != 0){
		//			waitpid(pcb[i].process_id, NULL, 0);
		//		}
		//	}
		//	break;
		//	msgctl(msg_queue_id, IPC_RMID, NULL);
		//	shmdt(clock_point);
		//	shmctl(clock_shmid, IPC_RMID, NULL);
		//fprintf(stderr, "PCB duration: %d\n", pcb[getpid()].id);
		//	shmdt(pcb);
		//	shmctl(pcb_shmid, IPC_RMID, NULL);
			//return 1;

		//}
		
    }
		//fprintf(stderr, "PCB duration: %d\n", pcb[getpid()].id);

	msgctl(msg_queue_id, IPC_RMID, NULL);
	shmdt(clock_point);
	shmctl(clock_shmid, IPC_RMID, NULL);
	shmdt(pcb);
	shmctl(pcb_shmid, IPC_RMID, NULL);
	shmctl(clock_shmid, IPC_RMID, NULL);
    shmctl(pcb_shmid, IPC_RMID, NULL);
	//shmctl(shmid_3, IPC_RMID, NULL);
	//	fprintf(stderr, "PCB duration: %d\n", pcb[getpid()].id);
    semctl(sem_id, 0, IPC_RMID);
    semctl(sem_id, 1, IPC_RMID);
	return 0;
}

struct queue* queue_generator(int numb_kids){
	
	struct queue* queue = (struct queue*) malloc(sizeof(struct queue));

    queue->max_kids = numb_kids;
    queue->beginning = 0;
	queue->size = 0;
    queue->end = numb_kids - 1;
    queue->array = (PCB*)malloc(queue->max_kids * sizeof(PCB));
   
	return queue;

}

int random_numb_gen(int min, int max) {

    return rand()%(max - min) + min;

}

US_P _init_user_process(int index, pid_t child_id){

	US_P* user_process = (struct user_process*) malloc(sizeof(struct user_process));
	user_process->id = index;
	user_process->process_id = child_id;
	user_process->priority = 0;

	int random_number = random_numb_gen(0, 3);
//	fprintf(stderr, "OSS: RANDOM NUMBER: %d\n", random_number);
   if(random_number == 0) {
       user_process->duration = 0;
       user_process->wait_time = 0;

   } else if(random_number == 1) {
       user_process->duration = QUANTUM;
       user_process->wait_time = 0;

   } else if(random_number == 2) {
       int seconds = random_numb_gen(0, 5);
	//	fprintf(stderr, "PCB duration: %d\n", pcb[getpid()].id);
       int milli_sec = random_numb_gen(0, 1000) * 1000000;
       int total_nanos = seconds * 1000000000 + milli_sec;
		

		//fprintf(stderr, "PCB duration: %d\n", pcb[getpid()].id);
       user_process->wait_time = milli_sec + total_nanos;
       user_process->duration = QUANTUM;

   } else if(random_number == 3) {
       double p_value = random_numb_gen(1, 99) / 100;
       user_process->duration = p_value * QUANTUM;
       user_process->wait_time = 0;
   }

    return *user_process;
}

PCB copy_user_to_pcb(PCB pcb, US_P user_process, int id){

	pcb.id = id;
    pcb.process_id = user_process.process_id;
	pcb.priority = user_process.priority;
	pcb.burst_time = user_process.burst_time;
    pcb.duration = user_process.duration;	
	pcb.wait_time = user_process.wait_time;

    return pcb;
}

void push_to_queue(struct queue* q, PCB pcb) {
		//fprintf(stderr, "PCB duration: %d\n", pcb[getpid()].id);
    if(q->size < q->max_kids) {
        q->end = (q->end+1)%q->max_kids;
        q->array[q->end] = pcb;
        q->size++;
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

int convert_to_millis(int q){

    return q / 1000000;

}

long int convert_to_ns(int q){

    return q * 1000000000;

}

void pop_from_queue(struct queue* q) {
   // PCB* item = NULL;

   // *item = queue->array[queue->front];
    q->beginning = (q->beginning + 1)%q->max_kids;
    q->size = q->size - 1;
    //return *item;

}

/*************** 
* Set up timer *
***************/
static int setuptimer(int time){
    struct itimerval value;
    value.it_value.tv_sec = time;
   value.it_value.tv_usec = 0;

    value.it_interval.tv_sec = 0;
    value.it_interval.tv_usec = 0;
    return(setitimer(ITIMER_REAL, &value, NULL));
}
 
/*******************
* Set up interrupt *
*******************/
static int setupinterrupt(){
    struct sigaction act;
    act.sa_handler = &myhandler;
    act.sa_flags = SA_RESTART;
    return(sigemptyset(&act.sa_mask) || sigaction(SIGALRM, &act, NULL));
}

/************************
* Set up my own handler *
************************/
static void myhandler(int s){
	fprintf(stderr, "\n!!!Termination begin since timer reached its time!!!\n");
	int i;
	for(i=0; i<20; i++){
        if(pcb[i].process_id != 0){
            if(kill(pcb[i].process_id, 0) == 0){
                if(kill(pcb[i].process_id, SIGTERM) != 0){
                    perror("Child can't be terminated for unkown reason\n");
                }
            }
        }
    }

    for(i=0;i<20;i++){
		if(pcb[i].process_id != 0){
            waitpid(pcb[i].process_id, NULL, 0);
        }
    }

//	for(i=0; i<20; i++){
//		if(child_arr[i] != 0){
//			if(kill(child_arr[i], 0) == 0){
//				if(kill(child_arr[i], SIGTERM) != 0){
//					perror("Child can't be terminated for unkown reason\n");
//				}
//			}
//		}
//	}

//	for(i=0;i<20;i++){
//		if(child_arr[i] != 0){
//			waitpid(child_arr[i], NULL, 0);
//		}
//	}

	quit = true;
}

