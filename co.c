#include "co.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdatomic.h>
#include <signal.h>
#ifdef __linux__
#include <sys/prctl.h>
#include <bits/types/sigset_t.h>
#include <bits/sigaction.h>
#else
// macOS includes
#include <signal.h>
#endif

void taskQueueAdd(task_func_t func, void *arg);
void *workerThreadFunction(void *arg);
task_t taskQueuePeek();
void co_shutdown();
void co_init();
int wait_sig();

typedef struct {
    task_t tasks[TASK_QUEUE_SIZE];
    int head;
    int tail;
} task_queue_t;

pthread_mutex_t lock;
pthread_cond_t cond;

task_queue_t task_queue = {.head = 0, .tail = 0};
pthread_t workers[WORKER_COUNT];
atomic_bool isRunning = true;
atomic_bool workers_init = false;


void co_init() {
    if (workers_init) return;
    int workerIds[WORKER_COUNT];
    for (int i = 0; i < WORKER_COUNT; i++) {
        workerIds[i] = i + 1;
    }
    for (int i = 0; i < WORKER_COUNT; i++) {
        int *threadId = malloc(sizeof(int));
        *threadId = i + 1;
        pthread_create(&workers[i], NULL, workerThreadFunction, (void*) threadId);
    }

    workers_init = true;
}

void co_shutdown() {
    isRunning = false;
    pthread_cond_broadcast(&cond);
    for (int i = 0; i < WORKER_COUNT; i++) {
        pthread_join(workers[i], NULL);
    }
}

void co(task_func_t func, void *arg) {
    taskQueueAdd(func, arg);
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

void taskQueueAdd(task_func_t func, void *arg) {
    pthread_mutex_lock(&lock);
    task_t task = {func, arg};
    task_queue.tasks[task_queue.tail] = task;
    task_queue.tail += 1;
    task_queue.tail %= TASK_QUEUE_SIZE;
    pthread_cond_signal(&cond);
    pthread_mutex_unlock(&lock);
}

task_t taskQueuePeek() {
    pthread_mutex_lock(&lock);
    while (task_queue.head == task_queue.tail && isRunning) {
        pthread_cond_wait(&cond, &lock);
    }
    task_t task = task_queue.tasks[task_queue.head];
    task_queue.head += 1;
    task_queue.head %= TASK_QUEUE_SIZE;
    pthread_mutex_unlock(&lock);
    return task;
}

void *workerThreadFunction(void *arg) {
    // setting the name for thread_name in co_ecample.c
    int workerId = *(int*) arg;
    char threadName[32];
    sprintf(threadName, "Worker id: %d", workerId);
    #ifdef __linux__
    prctl(PR_SET_NAME, threadName, 0, 0, 0);
    #endif

    while (isRunning) {
        task_t task = taskQueuePeek();
        if (task.func) {
            task.func(task.arg);
        }
    }

    free(arg);
    return NULL;
}