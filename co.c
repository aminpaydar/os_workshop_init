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
    task_t buffer[TASK_QUEUE_SIZE];
    int front, rear;
    pthread_mutex_t lock;
    pthread_cond_t not_empty;
} task_queue_t;

task_queue_t queue = {.front = 0, .rear = 0, .lock = PTHREAD_MUTEX_INITIALIZER, .not_empty = PTHREAD_COND_INITIALIZER};
pthread_t pool[WORKER_COUNT];
atomic_bool is_active = true;
atomic_bool initialized = false;

void task_queue_push(task_func_t fn, void *data) {
    pthread_mutex_lock(&queue.lock);
    task_t t = {fn, data};
    queue.buffer[queue.rear] = t;
    queue.rear = (queue.rear + 1) % TASK_QUEUE_SIZE;
    pthread_cond_signal(&queue.not_empty);
    pthread_mutex_unlock(&queue.lock);
}

task_t task_queue_pop() {
    pthread_mutex_lock(&queue.lock);
    while (queue.front == queue.rear && is_active) {
        pthread_cond_wait(&queue.not_empty, &queue.lock);
    }
    task_t t = queue.buffer[queue.front];
    queue.front = (queue.front + 1) % TASK_QUEUE_SIZE;
    pthread_mutex_unlock(&queue.lock);
    return t;
}

void *worker_thread(void *arg) {
    int id = *(int*) arg;
    char namebuf[32] = {0};
    snprintf(namebuf, sizeof(namebuf), "Worker-%d", id);
    prctl(PR_SET_NAME, namebuf, 0, 0, 0);

    while (is_active) {
        task_t task = task_queue_pop();
        if (task.func != NULL) {
            task.func(task.arg);
        }
    }

    free(arg);
    return NULL;
}

void co_init() {
    if (initialized) return;

    for (int i = 0; i < WORKER_COUNT; i++) {
        int *tid = malloc(sizeof(int));
        *tid = i + 1;
        pthread_create(&pool[i], NULL, worker_thread, tid);
    }

    initialized = true;
}

void co_shutdown() {
    is_active = false;
    pthread_cond_broadcast(&queue.not_empty);
    for (int i = 0; i < WORKER_COUNT; ++i) {
        pthread_join(pool[i], NULL);
    }
}

void co(task_func_t fn, void *arg) {
    task_queue_push(fn, arg);
}

int wait_sig() {
    sigset_t sigs;
    sigemptyset(&sigs);
    sigaddset(&sigs, SIGINT);
    sigaddset(&sigs, SIGTERM);
    pthread_sigmask(SIG_BLOCK, &sigs, NULL);

    printf("Waiting for SIGINT (Ctrl+C) or SIGTERM...\n");

    int signo = 0;
    sigwait(&sigs, &signo);

    printf("Received signal %d, shutting down...\n", signo);
    return signo;
}
