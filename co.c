#include "co.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdatomic.h>
#include <signal.h>
#include <pthread.h>
#ifdef __linux__
#include <sys/prctl.h>
#include <bits/types/sigset_t.h>
#include <bits/sigaction.h>
#else
#include <signal.h>
#endif

static task_t task_queue[TASK_QUEUE_SIZE];
static atomic_int queue_head = 0;
static atomic_int queue_tail = 0;
static pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t queue_not_empty = PTHREAD_COND_INITIALIZER;
static pthread_cond_t queue_not_full = PTHREAD_COND_INITIALIZER;

static pthread_t workers[WORKER_COUNT];
static atomic_bool running = true;

static void* worker_thread(void* arg) {
    while (running) {
        task_t* task = NULL;
        
        pthread_mutex_lock(&queue_mutex);
        while (queue_head == queue_tail && running) {
            pthread_cond_wait(&queue_not_empty, &queue_mutex);
        }
        
        if (queue_head != queue_tail) {
            task = &task_queue[queue_head % TASK_QUEUE_SIZE];
            queue_head++;
            pthread_cond_signal(&queue_not_full);
        }
        pthread_mutex_unlock(&queue_mutex);
        
        if (task) {
            task->func(task->arg);
        }
    }
    return NULL;
}

void co_init() {
    queue_head = 0;
    queue_tail = 0;
    
    for (int i = 0; i < WORKER_COUNT; i++) {
        pthread_create(&workers[i], NULL, worker_thread, NULL);
    }
}

void co_shutdown() {
    running = false;
    
    pthread_mutex_lock(&queue_mutex);
    pthread_cond_broadcast(&queue_not_empty);
    pthread_mutex_unlock(&queue_mutex);
    
    for (int i = 0; i < WORKER_COUNT; i++) {
        pthread_join(workers[i], NULL);
    }
    
    pthread_mutex_destroy(&queue_mutex);
    pthread_cond_destroy(&queue_not_empty);
    pthread_cond_destroy(&queue_not_full);
}

void co(task_func_t func, void *arg) {
    task_t newtask;
    newtask.arg = arg;
    newtask.func = func;
    
    pthread_mutex_lock(&queue_mutex);
    
    while ((queue_tail - queue_head) >= TASK_QUEUE_SIZE) {
        pthread_cond_wait(&queue_not_full, &queue_mutex);
    }
    
    task_queue[queue_tail % TASK_QUEUE_SIZE] = newtask;
    queue_tail++;
    
    pthread_cond_signal(&queue_not_empty);
    pthread_mutex_unlock(&queue_mutex);
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