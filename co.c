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

pthread_t threads[WORKER_COUNT];
task_t task_queue[TASK_QUEUE_SIZE];

int queue_count = 0;
int queue_head = 0;
int queue_tail = 0;

pthread_mutex_t queue_mutex;
pthread_cond_t queue_cond = PTHREAD_COND_INITIALIZER;

void *worker_loop(void *arg) {

    while (true) {
        task_t current;
        bool task_retrieved = false;

        pthread_mutex_lock(&queue_mutex);

        while (queue_count == 0) {
            pthread_cond_wait(&queue_cond, &queue_mutex);
        }

        if (queue_count > 0) {
            current = task_queue[queue_head];
            queue_head = (queue_head + 1) % TASK_QUEUE_SIZE;
            queue_count--;
            task_retrieved = true;
        }
        pthread_mutex_unlock(&queue_mutex);
        if (task_retrieved) {
            current.func(current.arg);
        }
    }
}

void co_init() {
    pthread_mutex_init(&queue_mutex, NULL);
    pthread_cond_init(&queue_cond, NULL);
    for (int i = 0; i < WORKER_COUNT; i++) {
        pthread_create(&threads[i], NULL, worker_loop, NULL);
    }
}

void co_shutdown() {
    for (int i = 0; i < WORKER_COUNT; i++) {
        pthread_join(threads[i], NULL);
    }
    pthread_mutex_destroy(&queue_mutex);
    pthread_cond_destroy(&queue_cond);
}

void co(task_func_t func, void *arg) {
    pthread_mutex_lock(&queue_mutex);

    task_queue[queue_tail].func = func;
    task_queue[queue_tail].arg = arg;
    queue_tail = (queue_tail + 1) % TASK_QUEUE_SIZE;
    queue_count++;

    pthread_cond_signal(&queue_cond);

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