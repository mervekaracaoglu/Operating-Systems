#ifndef WBQ_H
#define WBQ_H

#include <pthread.h>

// Structs and methods for WorkBalancerQueue, you can use additional structs
// and data structures ON TOP OF the ones provided here.

// **********************************************************

typedef struct WorkBalancerQueue WorkBalancerQueue;

typedef struct ThreadArguments {
    WorkBalancerQueue* q;
    int id;
} ThreadArguments;

typedef struct Task {
    char* task_id;
    int task_duration;
    double cache_warmed_up; //higher, better cache optimization, reducing the execution time
    WorkBalancerQueue* owner;
} Task;

typedef struct QueueNode {
    Task* job;
    struct QueueNode * next;

} QueueNode;

// TODO: You can modify this struct and add any
// fields you may need
struct WorkBalancerQueue {
    QueueNode* head;
    QueueNode* tail;
    int size;
    pthread_mutex_t head_lock, tail_lock;


};

// **********************************************************

// WorkBalancerQueue API **********************************************************
void submitTask(WorkBalancerQueue* q, Task* _task);
Task* fetchTask(WorkBalancerQueue* q);
Task* fetchTaskFromOthers(WorkBalancerQueue* q);
void queue_init(WorkBalancerQueue * q);
void print_queue(WorkBalancerQueue * q);
int total_jobs();
// You can add more methods to Queue API
// .
// .
// **********************************************************


// Your simulator threads should call this function to simulate execution.
// Don't change the function signature, you can use the provided implementation of
// this function. We will use potentially different implementations while testing.
void executeJob(Task* task, WorkBalancerQueue* my_queue, int my_id );

void* processJobs(void* arg);
void initSharedVariables();
#endif