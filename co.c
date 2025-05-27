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

typedef struct {
    task_t tasks[TASK_QUEUE_SIZE];
    int head, tail;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
} task_queue_t;

task_queue_t task_queue = {.head = 0, .tail = 0, .mutex = PTHREAD_MUTEX_INITIALIZER, .cond = PTHREAD_COND_INITIALIZER};
pthread_t workers[WORKER_COUNT];
atomic_bool running = true;
atomic_bool workers_init = false;

void task_queue_push(task_func_t func, void *arg) {
    pthread_mutex_lock(&task_queue.mutex);
    task_t task = {func, arg};
    task_queue.tasks[task_queue.tail] = task;
    task_queue.tail = (task_queue.tail + 1) % TASK_QUEUE_SIZE;
    pthread_cond_signal(&task_queue.cond);
    pthread_mutex_unlock(&task_queue.mutex);
}

task_t task_queue_pop() {
    pthread_mutex_lock(&task_queue.mutex);
    while (task_queue.head == task_queue.tail && running) {
        pthread_cond_wait(&task_queue.cond, &task_queue.mutex);
    }
    task_t task = task_queue.tasks[task_queue.head];
    task_queue.head = (task_queue.head + 1) % TASK_QUEUE_SIZE;
    pthread_mutex_unlock(&task_queue.mutex);
    return task;
}

void *worker_thread(void *arg) {
    int thread_id = *(int*) arg;
    char thread_name[32];
    sprintf(thread_name, "Worker-%d", thread_id);
    prctl(PR_SET_NAME, thread_name, 0, 0, 0);

    while (running) {
        task_t task = task_queue_pop();
        if (task.func) {
            task.func(task.arg);
        }
    }

    free(arg);
    return NULL;
}

void co_init() {
    if (workers_init) return;
    int worker_ids[WORKER_COUNT];
    for (int i = 0; i < WORKER_COUNT; i++) {
        worker_ids[i] = i + 1;
    }
    for (int i = 0; i < WORKER_COUNT; i++) {
        int *thread_id = malloc(sizeof(int));
        *thread_id = i + 1;
        pthread_create(&workers[i], NULL, worker_thread, (void*) thread_id);
    }

    workers_init = true;
}

void co_shutdown() {
    running = false;
    pthread_cond_broadcast(&task_queue.cond);
    for (int i = 0; i < WORKER_COUNT; i++) {
        pthread_join(workers[i], NULL);
    }
}

void co(task_func_t func, void *arg) {
    task_queue_push(func, arg);
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