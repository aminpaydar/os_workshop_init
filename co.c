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

typedef struct {
    task_t tasks[TASK_QUEUE_SIZE];
    int front;
    int rear;
    int count;
    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
    atomic_bool shutdown;
} task_queue_t;

static task_queue_t queue;
static pthread_t workers[WORKER_COUNT];

static void* worker_func(void* arg) {
    while (1) {
        pthread_mutex_lock(&queue.mutex);

        while (queue.count == 0 && !queue.shutdown) {
            pthread_cond_wait(&queue.not_empty, &queue.mutex);
        }

        if (queue.shutdown && queue.count == 0) {
            pthread_mutex_unlock(&queue.mutex);
            break;
        }

        task_t task = queue.tasks[queue.front];
        queue.front = (queue.front + 1) % TASK_QUEUE_SIZE;
        queue.count--;

        pthread_cond_signal(&queue.not_full);
        pthread_mutex_unlock(&queue.mutex);

        task.func(task.arg);
    }

    return NULL;
}

void co_init() {
    queue.front = 0;
    queue.rear = 0;
    queue.count = 0;
    queue.shutdown = false;

    pthread_mutex_init(&queue.mutex, NULL);
    pthread_cond_init(&queue.not_empty, NULL);
    pthread_cond_init(&queue.not_full, NULL);

    for (int i = 0; i < WORKER_COUNT; ++i) {
        pthread_create(&workers[i], NULL, worker_func, NULL);
    }
}

void co(task_func_t func, void *arg) {
    pthread_mutex_lock(&queue.mutex);

    while (queue.count == TASK_QUEUE_SIZE && !queue.shutdown) {
        pthread_cond_wait(&queue.not_full, &queue.mutex);
    }

    if (queue.shutdown) {
        pthread_mutex_unlock(&queue.mutex);
        return;
    }

    queue.tasks[queue.rear].func = func;
    queue.tasks[queue.rear].arg = arg;
    queue.rear = (queue.rear + 1) % TASK_QUEUE_SIZE;
    queue.count++;

    pthread_cond_signal(&queue.not_empty);
    pthread_mutex_unlock(&queue.mutex);
}

void co_shutdown() {
    pthread_mutex_lock(&queue.mutex);
    queue.shutdown = true;
    pthread_cond_broadcast(&queue.not_empty);
    pthread_mutex_unlock(&queue.mutex);

    for (int i = 0; i < WORKER_COUNT; ++i) {
        pthread_join(workers[i], NULL);
    }

    pthread_mutex_destroy(&queue.mutex);
    pthread_cond_destroy(&queue.not_empty);
    pthread_cond_destroy(&queue.not_full);
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