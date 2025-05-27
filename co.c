#include "co.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdatomic.h>
#include <signal.h>
#include <pthread.h>

typedef struct {
    task_t arr[TASK_QUEUE_SIZE];
    int front;
    int rear;
    int size;
    pthread_mutex_t lock;
    pthread_cond_t not_empty;
} Queue;

static Queue *task_queue_global = NULL;

void *worker_thread_func(void *arg) {
    Queue *task_queue = (Queue *)arg;

    pthread_mutex_lock(&task_queue->lock);
    while (true) {
        // Wait for a task to be available
        while (task_queue->size == 0) {
            pthread_cond_wait(&task_queue->not_empty, &task_queue->lock);
        }

        pthread_mutex_lock(&task_queue->lock);
        if (task_queue->size == 0) {
            pthread_mutex_unlock(&task_queue->lock);
            continue;  // No task available, continue waiting
        }
        task_t task = task_queue->arr[task_queue->front];
        task_queue->front = (task_queue->front + 1) % TASK_QUEUE_SIZE;
        task_queue->size--;
        pthread_mutex_unlock(&task_queue->lock);

        // Execute the task
        if (task.func != NULL) {
            task.func(task.arg);
        }
    }
    return NULL;
}

void co_init() {
    task_queue_global = malloc(sizeof(Queue));
    task_queue_global->front = 0;
    task_queue_global->rear = 0;
    task_queue_global->size = 0;
    
    pthread_mutex_init(&task_queue_global->lock, NULL);
    pthread_cond_init(&task_queue_global->not_empty, NULL);

    for (int i = 0; i < WORKER_COUNT; i++) {
        pthread_t worker_thread;
        pthread_create(&worker_thread, NULL, worker_thread_func, task_queue_global);
    }
}

void co_shutdown() {
    // TO BE IMPLEMENTED
}

void co(task_func_t func, void *arg) {
    task_t task;
    task.func = func;
    task.arg = arg;

    pthread_mutex_lock(&task_queue_global->lock);
    if (task_queue_global->size < TASK_QUEUE_SIZE) {
        task_queue_global->arr[task_queue_global->rear] = task;
        task_queue_global->rear = (task_queue_global->rear + 1) % TASK_QUEUE_SIZE;
        task_queue_global->size++;
        pthread_cond_signal(&task_queue_global->not_empty);  // Notify a worker thread
    } else {
        printf("Task queue is full, cannot add new task.\n");
    }
}

int wait_sig() {
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGTERM);
    pthread_sigmask(SIG_BLOCK, &mask, NULL);  // Block signals so they are handled by sigwait
    printf("Waiting for SIGINT (Ctrl+C) or SIGTERM...\n");
    int signum;
    sigwait(&mask, &signum);  // Wait for a signal
    printf("Received signal %d, shutting down...\n", signum);
    return signum;
}