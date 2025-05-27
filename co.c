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
    int front, rear, count;
    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
    bool shutdown;
} task_queue_t;

static pthread_t workers[WORKER_COUNT];
static task_queue_t task_queue;

void *worker_thread(void *arg) {
    while (true) {
        pthread_mutex_lock(&task_queue.mutex);

        while (task_queue.count == 0 && !task_queue.shutdown) {
            pthread_cond_wait(&task_queue.not_empty, &task_queue.mutex);
        }

        if (task_queue.shutdown) {
            pthread_mutex_unlock(&task_queue.mutex);
            break;
        }

        task_t task = task_queue.tasks[task_queue.front];
        task_queue.front = (task_queue.front + 1) % TASK_QUEUE_SIZE;
        task_queue.count--;

        pthread_cond_signal(&task_queue.not_full);
        pthread_mutex_unlock(&task_queue.mutex);

        task.func(task.arg);
    }
    return NULL;
}


void co_init() {
    task_queue.front = 0;
    task_queue.rear = 0;
    task_queue.count = 0;
    task_queue.shutdown = false;
    pthread_mutex_init(&task_queue.mutex, NULL);
    pthread_cond_init(&task_queue.not_empty, NULL);
    pthread_cond_init(&task_queue.not_full, NULL);

    for (int i = 0; i < WORKER_COUNT; i++) {
        pthread_create(&workers[i], NULL, worker_thread, NULL);
    }
}

void co_shutdown() {
    pthread_mutex_lock(&task_queue.mutex);
    task_queue.shutdown = true;
    pthread_cond_broadcast(&task_queue.not_empty);
    pthread_mutex_unlock(&task_queue.mutex);

    for (int i = 0; i < WORKER_COUNT; i++) {
        pthread_join(workers[i], NULL);
    }

    pthread_mutex_destroy(&task_queue.mutex);
    pthread_cond_destroy(&task_queue.not_empty);
    pthread_cond_destroy(&task_queue.not_full);
}

void co(task_func_t func, void *arg) {
    pthread_mutex_lock(&task_queue.mutex);

    while (task_queue.count == TASK_QUEUE_SIZE) {
        pthread_cond_wait(&task_queue.not_full, &task_queue.mutex);
    }

    task_queue.tasks[task_queue.rear].func = func;
    task_queue.tasks[task_queue.rear].arg = arg;
    task_queue.rear = (task_queue.rear + 1) % TASK_QUEUE_SIZE;
    task_queue.count++;

    pthread_cond_signal(&task_queue.not_empty);
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
