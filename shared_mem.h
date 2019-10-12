#pragma once
#ifndef HEADER_FILE
#define HEADER_FILE
#include <stdlib.h>

#define thresh_hold_oss 100000
#define thresh_hold_user 70000

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

} PCB;

typedef struct user_process {

    int id;
    pid_t process_id;
    int priority;
    int duration;
    int progress;
    int burst_time;
    int wait_time;

} US_P;

typedef struct queue {

    PCB* array;
    int beginning;
    int end;
    int size;
    unsigned int max_kids;

} QUE;

typedef struct message {

    int process_id;
    int done_flag;
    int id;
    int priority;
    int duration;
	int total_duration;
    int sec;
    int ns;
    char* message;

} Message;

#endif
