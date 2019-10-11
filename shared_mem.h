#pragma once
#ifndef HEADER_FILE
#define HEADER_FILE
#include <stdlib.h>

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

} pcb;

typedef struct user_process {

    int id;
    pid_t process_id;
    int priority;
    int duration;
    int progress;
    int burst_time;
    int wait_time;

} us_p;

typedef struct queue {

    pcb* array;
    int beginning;
    int end;
    int size;
    unsigned int max_kids;

} que;

#endif
