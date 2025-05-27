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


static pthread_t workers[WORKER_COUNT];
static task_t task_queue[TASK_QUEUE_SIZE];
static int task_count = 0;
static int queue_head = 0;
static int queue_tail = 0;
static pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t queue_cond_full = PTHREAD_COND_INITIALIZER;
static pthread_cond_t queue_cond_empty = PTHREAD_COND_INITIALIZER;
static atomic_bool shutdown_flag = false;
static atomic_bool initialized = false;

// The function each worker thread will execute
void *worker_thread(void *arg) {
    while (true) {
        pthread_mutex_lock(&queue_mutex);

        while (task_count == 0 && !shutdown_flag) {
            pthread_cond_wait(&queue_cond_empty, &queue_mutex);
        }

        if (shutdown_flag && task_count == 0) {
            pthread_mutex_unlock(&queue_mutex);
            break;
        }

        task_t task = task_queue[queue_head];
        queue_head = (queue_head + 1) % TASK_QUEUE_SIZE;
        task_count--;

        pthread_cond_signal(&queue_cond_full);
        pthread_mutex_unlock(&queue_mutex);

        task.func(task.arg);
    }
    return NULL;
}

// Initializes the thread pool
void co_init() {
    if (initialized) return;
    initialized = true;

    for (int i = 0; i < WORKER_COUNT; ++i) {
        pthread_create(&workers[i], NULL, worker_thread, NULL);
    }
}

// Shuts down the thread pool gracefully
void co_shutdown() {
    pthread_mutex_lock(&queue_mutex);
    shutdown_flag = true;
    pthread_cond_broadcast(&queue_cond_empty);
    pthread_mutex_unlock(&queue_mutex);

    for (int i = 0; i < WORKER_COUNT; ++i) {
        pthread_join(workers[i], NULL);
    }
}

// Submits a new task to the task queue
void co(task_func_t func, void *arg) {
    if (!initialized) {
        co_init();
    }

    pthread_mutex_lock(&queue_mutex);

    while (task_count == TASK_QUEUE_SIZE) {
        pthread_cond_wait(&queue_cond_full, &queue_mutex);
    }

    task_t task = { .func = func, .arg = arg };
    task_queue[queue_tail] = task;
    queue_tail = (queue_tail + 1) % TASK_QUEUE_SIZE;
    task_count++;

    pthread_cond_signal(&queue_cond_empty);
    pthread_mutex_unlock(&queue_mutex);
}

// Waits for SIGINT or SIGTERM to initiate shutdown
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