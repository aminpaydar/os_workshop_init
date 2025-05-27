#include "co.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdatomic.h>
#include <signal.h>
#ifdef __linux__
#include <sys/prctl.h>
//#include <bits/types/sigset_t.h>
//#include <bits/sigaction.h>
#else
// macOS includes
#include <signal.h>
#endif

static pthread_t workers[WORKER_COUNT];
static task_t task_queue[TASK_QUEUE_SIZE];

static int task_count = 0;
static int task_head = 0;
static int task_tail = 0;

static pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t queue_cond = PTHREAD_COND_INITIALIZER;
static bool shutdown_flag = false;


void* worker_thread(void* arg) {
    while (1) {
        pthread_mutex_lock(&queue_mutex);

        while (task_count == 0 && !shutdown_flag) {
            pthread_cond_wait(&queue_cond, &queue_mutex);
        }

        if (shutdown_flag && task_count == 0) {
            pthread_mutex_unlock(&queue_mutex);
            break;
        }

        task_t task = task_queue[task_head];
        task_head = (task_head + 1) % TASK_QUEUE_SIZE;
        task_count--;

        pthread_mutex_unlock(&queue_mutex);

        task.func(task.arg);
    }

    return NULL;
}

void co_init() {
    for (int i = 0; i < WORKER_COUNT; i++) {
        pthread_create(&workers[i], NULL, worker_thread, NULL);
    }


}

void co_shutdown() {
    pthread_mutex_lock(&queue_mutex);
    shutdown_flag = true;
    pthread_cond_broadcast(&queue_cond);
    pthread_mutex_unlock(&queue_mutex);

    for (int i = 0; i < WORKER_COUNT; i++) {
        pthread_join(workers[i], NULL);
    }
}

void co(task_func_t func, void *arg) {
    pthread_mutex_lock(&queue_mutex);

    if (task_count < TASK_QUEUE_SIZE) {
        task_queue[task_tail].func = func;
        task_queue[task_tail].arg = arg;
        task_tail = (task_tail + 1) % TASK_QUEUE_SIZE;
        task_count++;

        pthread_cond_signal(&queue_cond);
    } else {
        fprintf(stderr, "Task queue is full!\n");
    }

    pthread_mutex_unlock(&queue_mutex);
}

int wait_sig() {
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGTERM);
    pthread_sigmask(SIG_BLOCK, &mask, NULL);  
    printf("Waiting for SIGINT (Ctrl+C) or SIGTERM...\n");
    int signum;
    sigwait(&mask, &signum);  
    printf("Received signal %d, shutting down...\n", signum);
    return signum;
}