#pragma once
#ifndef HEADER_FILE
#define HEADER_FILE
#include <stdlib.h>

typedef struct Clock {
    unsigned int seconds;
    unsigned int nanoSeconds;
} simulated_clock;

typedef struct process_control_block {
    int pidIndex;
    pid_t actualPid;
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
    int index;
    pid_t actualPid;
    int priority;
    int duration;
    int progress;
    int burstTime;
    int waitTime;
} us_p;

typedef struct queue {
    PCB* array;
    int front;
    int rear;
    int size;
    unsigned int queueCapacity;
} que;

#endif
