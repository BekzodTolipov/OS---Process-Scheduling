#pragma once
#ifndef HEADER_FILE
#define HEADER_FILE
#include <stdlib.h>

#define thresh_hold_oss 100000
#define thresh_hold_user 70000
#define QUANTUM 50000
#define ALPHA 2.2
#define BETTA 2.8
#define MAX_PROCESSES 18

typedef struct Clock {

    unsigned int sec;
    unsigned int ns;

} simulated_clock;

typedef struct process_control_block {

    int id;
    pid_t process_id;
    int priority;
    int is_scheduled;
    int burst_time;
    int duration;//CPU time
    int wait_time;
	bool terminate;

} PCB;

typedef struct user_process {

    int id;
    pid_t process_id;
    int priority;
    int duration;
    int progress;
    int burst_time;
    int wait_time;
	bool terminate;

} US_P;

typedef struct QNode {

    int id;
	struct QNode *next;

} q_node;

typedef struct queue {

    struct QNode *front;
	struct QNode *rear;

} QUE;

typedef struct message {

	long int mtype;
    int process_id;
    int done_flag;
    int id;
    int priority;
    int duration;
	int burst_time;
    int sec;
    int ns;
    char* message;
	int wait_time;

} Message;

#endif
