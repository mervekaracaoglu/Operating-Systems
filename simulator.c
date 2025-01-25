#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <limits.h>

#include "constants.h"
#include "wbq.h"

extern int stop_threads;
extern int finished_jobs[NUM_CORES];
extern WorkBalancerQueue** processor_queues;

// Thread function for each core simulator thread
void* processJobs(void* arg) {
    // Initialize local variables
    ThreadArguments* my_arg = (ThreadArguments*)arg;
    WorkBalancerQueue* my_queue = my_arg->q;
    int my_id = my_arg->id;
    const int HW = 20; // High watermark for load balancing
    const int LW = 10; // Low watermark for load balancing

    while (!stop_threads) {
        // Fetch task from the owner's queue

        Task* task = fetchTask(my_queue);
        // If no task in owner's queue, try to fetch from other cores
        if (!stop_threads && task == NULL) {
            for (int i = 0; i < NUM_CORES; i++) {
                if (i != my_id) { // Search for idle cores
                    task = fetchTaskFromOthers(processor_queues[i]);
                    if (task) {
                        task->cache_warmed_up = 0.0;
                        break;
                    }
                }
            }
        }

        // Process the fetched task
        if (!stop_threads && task != NULL) {

            executeJob(task, my_queue, my_id); // Simulate task execution

            // If task is not finished, reinsert it into the owner's queue
            if (task->task_duration > 0) {
                submitTask(my_queue, task);
            } else {
                free(task->task_id);
                free(task); // Free completed task
                task = NULL;
                finished_jobs[my_id]++;
            }
        } else if(!stop_threads) {
            usleep(1000);// Sleep if no task is available
        }

        // Load balancing: Offload tasks if queue size exceeds HW
        int queue_size = my_queue->size;
        if (!stop_threads && queue_size > HW) {
            Task* excess_task = fetchTask(my_queue);
            if (excess_task) {
                for (int i = 0; i < NUM_CORES; i++) {
                    if (i != my_id && processor_queues[i]  -> size < LW) {
                        submitTask(processor_queues[i], excess_task); // Offload task
                        break;
                    }
                }
            }
        }

        // Check the stop flag
        if (stop_threads) {
            break;
        }
    }

    free(my_arg);
    my_arg = NULL;// Free the ThreadArguments structure
    return NULL;
}

// Do any initialization of your shared variables here.
// For example initialization of your queues, any data structures
// you will use for synchronization etc.
void initSharedVariables() {
    for (int i = 0; i < NUM_CORES; i++) {
        // Allocate memory for each processor queue
        processor_queues[i] = malloc(sizeof(WorkBalancerQueue));

        // Initialize each queue using Michael-Scott queue initialization
        queue_init(processor_queues[i]);

        // Initialize finished jobs count
        finished_jobs[i] = 0;
    }

    stop_threads = 0; // Initialize the stop flag
}


