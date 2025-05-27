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
    int head, tail;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
} work_queue_t;

work_queue_t work_queue = {.head = 0, .tail = 0, .mutex = PTHREAD_MUTEX_INITIALIZER, .cond = PTHREAD_COND_INITIALIZER};
pthread_t threads[WORKER_COUNT];
atomic_bool running = true;
atomic_bool init_done = false;

void queue_add(task_func_t func, void *arg) {
    pthread_mutex_lock(&work_queue.mutex);
    task_t task = {func, arg};
    work_queue.tasks[work_queue.tail] = task;
    work_queue.tail = (work_queue.tail + 1) % TASK_QUEUE_SIZE;
    pthread_cond_signal(&work_queue.cond);
    pthread_mutex_unlock(&work_queue.mutex);
}

task_t queue_get() {
    pthread_mutex_lock(&work_queue.mutex);
    while (work_queue.head == work_queue.tail && running) {
        pthread_cond_wait(&work_queue.cond, &work_queue.mutex);
    }
    task_t task = work_queue.tasks[work_queue.head];
    work_queue.head = (work_queue.head + 1) % TASK_QUEUE_SIZE;
    pthread_mutex_unlock(&work_queue.mutex);
    return task;
}

void *worker(void *arg) {
    int id = *(int*) arg;
    char name[32];
    sprintf(name, "Thread-%d", id);
    prctl(PR_SET_NAME, name, 0, 0, 0);
    while (running) {
        task_t task = queue_get();
        if (task.func) {
            task.func(task.arg);
        }
    }
    free(arg);
    return NULL;
}

void co_init() {
    if (init_done) {
        return;
    }
    int ids[WORKER_COUNT];
    for (int i = 0; i < WORKER_COUNT; i++) {
        ids[i] = i;
    }
    for (int i = 0; i < WORKER_COUNT; i++) {
        int *id = malloc(sizeof(int));
        *id = i;
        pthread_create(&threads[i], NULL, worker, (void*) id);
    }
    init_done = true;
}

void co_shutdown() {
    running = false;
    pthread_cond_broadcast(&work_queue.cond);
    for (int i = 0; i < WORKER_COUNT; i++) {
        pthread_join(threads[i], NULL);
    }
}

void co(task_func_t func, void *arg) {
    queue_add(func, arg);
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