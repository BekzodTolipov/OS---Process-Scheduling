/********************************************************************************************  
 *		Program Description: Simple program demonstration of inter process communication	*
 *      by using following system calls(fork, exec, shared memory, and semaphore, message	*
 *		queue) and Process scheduling using simple queueing system using round robin logic	*
 *		"oss" will act as operating system and spawn children process in total 18 and		*
 *		dispatch child process one at a time according to which queue there in.				*
 *		"user" will be individual processes and will be making busy wait by decision it		*
 *		made if its full quantum or half													*
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
#include <stdint.h>
#include "shared_mem.h"

//Prototypes
QUE* queue_generator();
int random_numb_gen(int min, int max);
US_P *_init_user_process(int index, pid_t child_id);
void copy_user_to_pcb(PCB *pcb, US_P *user_process);
void push_to_queue(struct queue* queue, int id);
struct QNode * pop_from_queue(QUE* queue);
void fix_time();
void sem_clock_lock();
void sem_clock_release();
int convert_to_millis(int q);
long int convert_to_ns(int q);
static int setuptimer(int s);
static int setupinterrupt();
static void myhandler(int s);
void fix_time();
static void terminator();
int toint(char str[]);

////////////////

//Global variables and shared memory
PCB* pcb;
simulated_clock* sim_clock;
static struct queue* low_queue;
static struct queue* med_queue;
static struct queue* high_queue;
int msg_queue_id;
int *check_permission;
simulated_clock* clock_point;
int pcb_shmid;
pid_t parent_pid;
pid_t child_id;
static bool quit;
int sem_id;
//Semaphore set up
struct sembuf sem_op;
int clock_shmid;
long int total_ns;
static Message msg;
//char *shmMsg;
static unsigned char bit_map[3];
#define MAXCHAR 1024

int main(int argc, char **argv){

	char file_name[MAXCHAR] = "log.dat";
	char dummy[MAXCHAR];
	//int numb_child = 5;
	int max_time = 10;
	int c;
	quit = false;
	
	parent_pid = getpid();
	// Read the arguments given in terminal
	while ((c = getopt (argc, argv, "hs:l:t:")) != -1){
		switch (c)
		{
			case 'h':
				printf("To run the program you have following options:\n\n[ -h for help]\n[ -l filename ]\n[ -t time in seconds ]\nTo execute the file follow the code:\n./%s [ -h ] or any other options", argv[0]);
				return 0;
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

	msg_queue_id = msgget(msg_queue_key, IPC_CREAT | 0666);

	if ((clock_shmid = shmget(clock_key, sizeof(simulated_clock), IPC_CREAT | 0600)) < 0) {
        fprintf(stderr, "ERROR: shmat failed on shared message");
		return 1;
    }

    clock_point = shmat(clock_shmid, NULL, 0);
	
	//Set up starter values for clock and message signal
	clock_point->sec = 0;
	clock_point->ns = 0;

	//Set up pcb starter
    if ((pcb_shmid = shmget(pcb_key, sizeof(struct process_control_block) * MAX_PROCESSES, IPC_CREAT | 0666)) < 0) {
        fprintf(stderr, "shmat failed on shared message");
		return 1;
    }
	//Attach shared mem to pcb
    pcb = shmat(pcb_shmid, NULL, 0);

	//Set up queues high, medium, low
	high_queue = queue_generator();
	med_queue = queue_generator();
	low_queue = queue_generator();

	//int spawning = 0;
	//int prev_clock = 0;
	//int queue_lvl = -1;
	int id = 0;
	//int start_ns = 0;
	int which_queue = 0;
	//int total_cpu_spent_user = 0;
	//int total_cpu_spent_queue = 0;
	//long int start_time = 0;
	char index_str[MAXCHAR];
	int total_kids = 0;
	float med_queue_wait_time = 0.0;
	float low_queue_wait_time = 0.0;
	bool bit_accessable;
	while(!quit) {
		//Is this spot in the bit map open?
		bit_accessable = false;
		int count = 0;
		while(1){
			id = (id + 1) % 3;
			uint32_t bit = bit_map[id / 8] & (1 << (id % 8));
			if(bit == 0){
				bit_accessable = true;
				break;
			}
			else{
				bit_accessable = false;
			}

			if(count >= 3 - 1){
				fprintf(stderr, "OSS: bitmap is full\n");
				fprintf(fptr, "OSS: bitmap is full\n");
				fflush(fptr);
				break;
			}
			count++;
		}
		//start_time = convert_to_ns(clock_point->sec) + clock_point->ns;
		if(total_kids < 100){
			if(bit_accessable) {
				//spawning = random_numb_gen(0, 2);
				//if((clock_point->sec - prev_clock) >= spawning) {	//Tried to sapwn children at different time but its breaking
					child_id = fork();

					if(child_id == 0) {	//Child process
						//Execute ./child
						execl("./child", "./child", index_str, NULL);
					} 
					else if (child_id < 0) {	//Failed fork()
						terminator();
						break;
					}
					else{	//Parent process
						total_kids++;
						bit_map[id / 8] |= (1 << (id % 8));	//Set bitmap spot to 1
						US_P* user_process = _init_user_process(id, child_id);	//Initialize the user process

						copy_user_to_pcb(&pcb[id], user_process);	//Copy the user_process data to process control block
						//Print it to screen and file
						fprintf(stderr, "\nOSS: Generating process with PID %d and putting it in queue %d at time %d.%d\n", pcb[id].process_id, pcb[id].priority, clock_point->sec, clock_point->ns);
						fprintf(fptr, "\nOSS: Generating process with PID %d and putting it in queue %d at time %d.%d\n", pcb[id].process_id, pcb[id].priority, clock_point->sec, clock_point->ns);
						fflush(fptr);

						push_to_queue(high_queue, id);
						//Set up priority
						pcb[id].priority = 0;
					}
			//	}
			}
		}
		else{ //Terminate if reached 100 kids
			fprintf(stderr, "FINISH: Reached max 100 kids\n");
			terminator();
			break;
			
		}
		//Identify next working process in the queue
		struct QNode next_working_process;
		if(which_queue == 0){
			next_working_process.next = high_queue->front;
		}
		else if(which_queue == 1){
			next_working_process.next = med_queue->front;
		}
		else{
			next_working_process.next = low_queue->front;
		}

		int total_processes_in_queue = 0;
		float total_wait_time_in_queue = 0.0;
		struct queue *temp_queue = queue_generator();	//Temporarly create queue
		int working_process_id;
		while(next_working_process.next != NULL){
			total_processes_in_queue++;
			
			//--------------------------------//
			//Increment Clock
			sem_clock_lock();
			//increment seconds
			clock_point->ns += 1000;
			fix_time();
			// Release the critical section
			sem_clock_release();
			//--------------------------------//
			
			working_process_id = next_working_process.next->id;
			msg.mtype = pcb[working_process_id].process_id;
			msg.id = working_process_id;
			msg.process_id = pcb[working_process_id].process_id;
			msg.priority = which_queue;
			pcb[working_process_id].priority = which_queue;
			//Send message to user.c
			msgsnd(msg_queue_id, &msg, (sizeof(Message) - sizeof(long)), 0);
			//Print to screen and file
			fprintf(stderr, "OSS: Signaling process with PID (%d) from queue -%d- to dispatch\n", msg.process_id, which_queue);	
			fprintf(fptr, "OSS: Signaling process with PID (%d) from queue -%d- to dispatch\n", msg.process_id, which_queue);
			fflush(fptr);
			
			//--------------------------------//
			//Increment Clock
			sem_clock_lock();
			//increment seconds

			clock_point->ns += 1000;
			fix_time();
			// Release the critical section
			sem_clock_release();
			//--------------------------------//
			
			msgrcv(msg_queue_id, &msg, (sizeof(Message) - sizeof(long)), 1, 0);
			//Print to screen and file
			fprintf(stderr, "OSS: Dispatching process with PID (%d) from queue -%d- at time %d.%d\n", msg.process_id, which_queue, msg.sec, msg.ns);
			fprintf(fptr, "OSS: Dispatching process with PID (%d) from queue -%d- at time %d.%d\n", msg.process_id, which_queue, msg.sec, msg.ns);
			fflush(fptr);
			
			//--------------------------------//
			//Increment Clock
			sem_clock_lock();
			//increment seconds
			clock_point->ns += 1000;
			fix_time();
			// Release the critical section
			sem_clock_release();
			//--------------------------------//
			
			total_ns = convert_to_ns(clock_point->sec) + clock_point->ns - convert_to_ns(msg.sec) + msg.ns;
			//Print to screen and file
			fprintf(stderr, "OSS: total time this dispatch was %li nanoseconds\n", total_ns);
			fprintf(fptr, "OSS: total time this dispatch was %li nanoseconds\n", total_ns);
			fflush(fptr);

			while(1){	//Increment clock while process in critical section
				
				//--------------------------------//
				//Increment Clock
				sem_clock_lock();
				//increment seconds
				clock_point->ns += 1000;
				fix_time();
				// Release the critical section
				sem_clock_release();
				//--------------------------------//
				
				int left_critical_section = msgrcv(msg_queue_id, &msg, (sizeof(Message) - sizeof(long)), 1, IPC_NOWAIT);
				if(left_critical_section != -1){
					fprintf(stderr, "OSS: Receiving that process with PID (%d) ran for %li nanoseconds\n", msg.process_id, (convert_to_ns(msg.sec) + msg.ns));
					fprintf(fptr, "OSS: Receiving that process with PID (%d) ran for %li nanoseconds\n", msg.process_id, (convert_to_ns(msg.sec) + msg.ns));
					fflush(fptr);
					break;
				}
			}
			
			//--------------------------------//
			//Increment Clock
			sem_clock_lock();
			//increment seconds
			clock_point->ns += 1000;
			fix_time();
			// Release the critical section
			sem_clock_release();
			//--------------------------------//
			
			//Blocked wait for message from user.c
			msgrcv(msg_queue_id, &msg, (sizeof(Message) - sizeof(long)), 1, 0);
			if(msg.done_flag == 0){	//Check if child finished running
				//Print to screen and file
				fprintf(stderr, "OSS: Process with PID (%d) has finish running at my time %d.%d\n", msg.process_id, clock_point->sec, clock_point->ns);
				fprintf(fptr, "OSS: Process with PID (%d) has finish running at my time %d.%d\n", msg.process_id, clock_point->sec, clock_point->ns);
				fflush(fptr);
				total_wait_time_in_queue += msg.wait_time;
			}
			else{
				if(which_queue == 0)
				{
					if(msg.wait_time > (ALPHA * med_queue_wait_time))
					{
						//Print to screen and file
						fprintf(stderr, "OSS: Putting process with PID (%d) to queue -1-\n", msg.process_id);
						fprintf(fptr, "OSS: Putting process with PID (%d) to queue -1-\n", msg.process_id);
						fflush(fptr);
						push_to_queue(med_queue, working_process_id);
						pcb[working_process_id].priority = 1;
					}
					else
					{
						//Print to screen and file
						fprintf(stderr, "OSS: Not using its entire time quantum. Putting process with PID (%d) to queue -0-\n", msg.process_id);
						fprintf(fptr, "OSS: Not using its entire time quantum. Putting process with PID (%d) to queue -0-\n", msg.process_id);
						fflush(fptr);


						push_to_queue(temp_queue, working_process_id);
						pcb[working_process_id].priority = 0;
					}
				}
				else if(which_queue == 1)
				{
					if(msg.wait_time > (BETTA * low_queue_wait_time))
					{
						//Print to screen and file
						fprintf(stderr, "OSS: Putting process with PID (%d) to queue -2-\n", msg.process_id);
						fprintf(fptr, "OSS: Putting process with PID (%d) to queue -2-\n", msg.process_id);
						fflush(fptr);

						push_to_queue(low_queue, working_process_id);
						pcb[working_process_id].priority = 2;
					}
					else
					{
						//Print to screen and file
						fprintf(stderr, "OSS: Not using its entire time quantum. Putting process with PID (%d) to queue -1-\n", msg.process_id);
						fprintf(fptr, "OSS: Not using its entire time quantum. Putting process with PID (%d) to queue -1-\n", msg.process_id);
						fflush(fptr);

						push_to_queue(temp_queue, working_process_id);
						pcb[working_process_id].priority = 1;
					}
				}
				else{
					//Print to screen and file
					fprintf(stderr, "OSS: Keeping process with PID (%d) in queue -2-\n", msg.process_id);
					fprintf(fptr, "OSS: Keeping process with PID (%d) in queue -2-\n", msg.process_id);
					fflush(fptr);

					push_to_queue(temp_queue, working_process_id);
					pcb[working_process_id].priority = 2;
				}

				total_wait_time_in_queue += msg.wait_time;
			}
			
			//Set up pointer to next process if exists
			if(next_working_process.next->next != NULL){
				next_working_process.next = next_working_process.next->next;
			}
			else{
				next_working_process.next = NULL;
			}
		}
		
		//Calculate threshhold wait time for next queue
		if(total_processes_in_queue == 0){
			total_processes_in_queue = 1;
		}

		if(which_queue == 1){
			med_queue_wait_time = (total_wait_time_in_queue / total_processes_in_queue);
		}
		else if(which_queue == 2){
			low_queue_wait_time = (total_wait_time_in_queue / total_processes_in_queue);
		}
		
		//Clear up queues as needed
		int temp_id = 0;
		if(which_queue == 0){
			while(high_queue->rear != NULL)
			{
				pop_from_queue(high_queue);
			}
			while(temp_queue->rear != NULL)
			{
				temp_id = temp_queue->front->id;
				push_to_queue(high_queue, temp_id);
				pop_from_queue(temp_queue);
			}
		}
		else if(which_queue == 1){
			while(med_queue->rear != NULL)
			{
				pop_from_queue(med_queue);
			}
			while(temp_queue->rear != NULL)
			{
				temp_id = temp_queue->front->id;
				push_to_queue(med_queue, temp_id);
				pop_from_queue(temp_queue);
			}
		}
		else{
			while(low_queue->rear != NULL)
			{
				pop_from_queue(low_queue);
			}
			while(temp_queue->rear != NULL)
			{
				temp_id = temp_queue->front->id;
				push_to_queue(low_queue, temp_id);
				pop_from_queue(temp_queue);
			}
		}
		free(temp_queue);
		
		which_queue = (which_queue + 1) % 3;
		
		//--------------------------------//
		//Increment Clock
		sem_clock_lock();
		//increment seconds
		clock_point->ns += 1000;
		fix_time();
		// Release the critical section
		sem_clock_release();
		//--------------------------------//

		int stat;
		pid_t remove_pid = waitpid(-1, &stat, WNOHANG);	// Non block wait for parent
		// If somebody died then barry them underground
		// and remove them from history
		if(remove_pid > 0){
			int pos;
			for(pos=0; pos<18;pos++){
				if(pcb[pos].process_id == remove_pid){
					bit_map[pcb[pos].id / 8] &= ~(1 << (pcb[pos].id % 8));
				}
			}
		}
	//	prev_clock = clock_point->sec;	//Needed for random spawning
	}
	//Clear the shared memory, message queue and semaphores
	msgctl(msg_queue_id, IPC_RMID, NULL);
	shmdt(clock_point);
	shmctl(clock_shmid, IPC_RMID, NULL);
	shmdt(pcb);
	shmctl(pcb_shmid, IPC_RMID, NULL);
	shmctl(clock_shmid, IPC_RMID, NULL);
    shmctl(pcb_shmid, IPC_RMID, NULL);
    semctl(sem_id, 0, IPC_RMID);
    semctl(sem_id, 1, IPC_RMID);
	return 0;
}

/*******************************************************
* Function is designed to convert string to an integer *
*******************************************************/
int toint(char str[])
{
    int len = strlen(str);
    int i, num = 0;

    for (i = 0; i < len; i++)
    {
        num = num + ((str[len - (i + 1)] - '0') * pow(10, i));
    }

   return num;
}

/*****************************************************
* Function to set up queue as front and rear to null *
*****************************************************/
QUE* queue_generator(){
	
	QUE* starter = (struct queue*) malloc(sizeof(struct queue));

    starter->front = NULL;
	starter->rear = NULL;
   
	return starter;

}
/***********************************************
* Function to generate random number generator *
***********************************************/
int random_numb_gen(int min, int max) {

    return rand()%(max - min) + min;

}
/*********************************************************
* Function to set up user process with its index and pid *
*********************************************************/
US_P *_init_user_process(int index, pid_t child_id){

	US_P* user_process = (struct user_process*) malloc(sizeof(struct user_process));
	user_process->id = index;
	user_process->process_id = child_id;
	user_process->priority = 0;
	
	return user_process;
}
/*********************************************************
* Function to copy user process to process control block *
*********************************************************/
void copy_user_to_pcb(PCB *pcb, US_P *user_process){

	pcb->id = user_process->id;
    pcb->process_id = user_process->process_id;
	pcb->priority = user_process->priority;
	
}
/*********************************************************
* Function to push process at the beginning of the queue *
*********************************************************/
void push_to_queue(struct queue* q, int id) {
	
	q_node *new_node = (struct QNode *)malloc(sizeof(struct QNode));
	new_node->id = id;
	new_node->next = NULL;
	
	//If queue is empty, then new node is front and rear both
	if(q->rear == NULL){
		q->front = q->rear = new_node;
		return;
	}

	//Add the new node at the end of queue and change rear 
	q->rear->next = new_node;
	q->rear = new_node;
	
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
/*************************************************
* Function to convert nanosecond to milli second *
*************************************************/
int convert_to_millis(int q){

    return q / 1000000;

}
/*******************************************
* Function to convert second to nanosecond *
*******************************************/
long int convert_to_ns(int q){

    return q * 1000000000;

}
/***************************************************
* Function to pop the given process from the queue *
***************************************************/
q_node *pop_from_queue(QUE* q) {
   
   if(q->front == NULL){	//If front is null than queue is empty
		return NULL;
	}
	//temporarly store front of the queue so free that memory later
	q_node *temp = q->front;
	free(temp);
	
	q->front = q->front->next;

	if (q->front == NULL)
	{
		q->rear = NULL;
	}

	return temp;

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

	msgctl(msg_queue_id, IPC_RMID, NULL);
	shmdt(clock_point);
    shmctl(clock_shmid, IPC_RMID, NULL);
    shmdt(pcb);
    shmctl(pcb_shmid, IPC_RMID, NULL);
    shmctl(clock_shmid, IPC_RMID, NULL);
    shmctl(pcb_shmid, IPC_RMID, NULL);
    semctl(sem_id, 0, IPC_RMID);
    semctl(sem_id, 1, IPC_RMID);
    abort();

}

/************************
* Set up terminator *
************************/
static void terminator(){
	fprintf(stderr, "\n!!!Termination begin!!!\n");
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

	quit = true;
}
