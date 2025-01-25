#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <assert.h>
#include "wbq.h"

// Do your WorkBalancerQueue implementation here.
// Implement the 3 given methods here. You can add
// more methods as you see necessary.
void queue_init(WorkBalancerQueue * q){
    QueueNode * dummy = malloc(sizeof(QueueNode));
    dummy -> next = NULL;
    q -> head = q-> tail = dummy;
    pthread_mutex_init(&q -> head_lock, NULL);
    pthread_mutex_init(&q -> tail_lock, NULL);
}

void submitTask(WorkBalancerQueue* queue, Task* task) {
    QueueNode* new_node = malloc(sizeof(QueueNode));
    assert(new_node != NULL);
    new_node->job = task;

    pthread_mutex_lock(&queue->tail_lock);
    queue -> tail -> next = new_node;
    queue -> tail = new_node;
    queue->size++;
    pthread_mutex_unlock(&queue->tail_lock);
}


Task* fetchTask(WorkBalancerQueue* queue) {
    pthread_mutex_lock(&queue->head_lock); // Lock the tail for exclusive access

    if (queue->head == queue->tail) {
        return NULL; // Queue is empty
    }

    QueueNode * temp = queue -> head;
    QueueNode *new_head = temp -> next;
    if(new_head == NULL){
        pthread_mutex_unlock(&queue -> head_lock);
        return NULL;
    }
    Task * fetched_job = new_head -> job;
    queue -> head = new_head;
    pthread_mutex_unlock(&queue -> head_lock);

    free(temp);
    return fetched_job;

}



Task* fetchTaskFromOthers(WorkBalancerQueue* queue) {
    pthread_mutex_lock(&queue->tail_lock); // Lock the tail for exclusive access

    // Check if the queue is empty or has only the dummy node
    if (queue->tail == queue->head) {
        pthread_mutex_unlock(&queue->tail_lock); // Unlock before returning
        return NULL;
    }

    // Start traversal from the head
    QueueNode* current = queue->head;

    // Traverse to the second-to-last node (node before the tail)
    while (current->next != queue->tail) {
        current = current->next;
    }

    // Detach the tail node
    QueueNode* tail_node = queue->tail; // Current tail
    Task* task = tail_node->job;        // Retrieve the task
    queue->tail = current;              // Update the tail pointer
    current->next = NULL;               // Disconnect the old tail

    free(tail_node); // Free the detached node

    pthread_mutex_unlock(&queue->tail_lock); // Unlock the tail

    return task; // Return the fetched task
}




/* Task* fetchTaskFromOthers(WorkBalancerQueue *queue): This method
 allows a core to remove a job from the tail end of another core’s queue.
 This method will not be called by the owner thread. When the WBQ is
 empty it returns NULL */

// Function to print the contents of a WorkBalancerQus