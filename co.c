// negin mosaei jo 40213434
//payam divsalar 40213012

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
#include <pthread.h>
#endif

typedef struct {
    task_t tasks[TASK_QUEUE_SIZE];
    int head, tail;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
} Task_queue;

Task_queue task_queue = {.head = 0, .tail = 0, .mutex = PTHREAD_MUTEX_INITIALIZER, .cond = PTHREAD_COND_INITIALIZER};


static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t has_job = PTHREAD_COND_INITIALIZER;
static pthread_cond_t has_space = PTHREAD_COND_INITIALIZER;
static pthread_t threads[WORKER_COUNT];
static int active = 1;


pthread_t thread_pool[WORKER_COUNT];

void *worker_thead (void *arg){
    while (active) {

        pthread_mutex_lock(&task_queue.mutex);

        while (active && task_queue.head == task_queue.tail)
            pthread_cond_wait(&task_queue.cond, &task_queue.mutex);

        task_t current = task_queue.tasks[task_queue.head];
        task_queue.head = (task_queue.head + 1) % TASK_QUEUE_SIZE;

        pthread_mutex_unlock(&task_queue.mutex);

        if (current.func)
            current.func(current.arg);
    }
    
    return NULL;
}


void co_init() {
    for(int i = 0; i < WORKER_COUNT; i++){
        pthread_create(&thread_pool[i], NULL, worker_thead , NULL);
    }

}

void co_shutdown() {
    active=0;

    pthread_cond_broadcast(&task_queue.cond);
   
    for (int i = 0; i < WORKER_COUNT; i++) {
        pthread_join(thread_pool[i], NULL);
    }
}

void co(task_func_t func, void *arg) {
    pthread_mutex_lock(&task_queue.mutex);
    
    task_t task;
    task.func = func;
    task.arg = arg;
    task_queue.tasks[task_queue.tail] = task;
    task_queue.tail = (task_queue.tail + 1) % TASK_QUEUE_SIZE;
    pthread_cond_signal(&task_queue.cond);
    pthread_mutex_unlock(&task_queue.mutex);

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
