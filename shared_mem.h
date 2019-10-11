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
    int isScheduled;
    int isBlocked;
    int burstTime;
    int resumeTime;
    int duration;
    int progress;
    int waitTime;

} pcb;

typedef struct user_process {

    int id;
    pid_t process_id;
    int priority;
    int duration;
    int progress;
    int burstTime;
    int waitTime;

} us_p;

typedef struct queue {

    pcb* array;
    int beginning;
    int end;
    int size;
    unsigned int max_kids;

} que;

#endif
