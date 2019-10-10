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


////////////////

pcb* pcb;
simulated_clock* sim_clock;
que* low_queue;
que* med_queue;
que* high_queue;
