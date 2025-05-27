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
#define MAX_TASKS 1024
#define NUM_THREADS 32

typedef struct {
    task_func_t func;
    void *arg;
} Task;

static Task task_queue[MAX_TASKS];
static int task_count = 0;
static int task_index = 0;
static pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t queue_cond = PTHREAD_COND_INITIALIZER;
static pthread_t threads[NUM_THREADS];
static int running = 1;

void *worker(void *arg) {
    while (1) {
        pthread_mutex_lock(&queue_mutex);

        while (task_index >= task_count && running) {
            pthread_cond_wait(&queue_cond, &queue_mutex);
        }

        if (!running && task_index >= task_count) {
            pthread_mutex_unlock(&queue_mutex);
            break;
        }

        Task task = task_queue[task_index++];
        pthread_mutex_unlock(&queue_mutex);

        task.func(task.arg);
    }
    return NULL;
}
void co_init() {
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_create(&threads[i], NULL, worker, NULL);
    }
}

void co(task_func_t func, void *arg) {
    pthread_mutex_lock(&queue_mutex);
    if (task_count < MAX_TASKS) {
        task_queue[task_count].func = func;
        task_queue[task_count].arg = arg;
        task_count++;
        pthread_cond_signal(&queue_cond);
    }
    pthread_mutex_unlock(&queue_mutex);
}

void co_shutdown() {
    pthread_mutex_lock(&queue_mutex);
    running = 0;
    pthread_cond_broadcast(&queue_cond);
    pthread_mutex_unlock(&queue_mutex);

    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
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