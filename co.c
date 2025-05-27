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

pthread_mutex_t lock1;

typedef struct {
    task_t Q[TASK_QUEUE_SIZE];
    int head;
    int tail;
    pthread_mutex_t lock;
    pthread_cond_t cond;
} Queue;

typedef struct ThreadPool {
    int head;
    int tail;
    pthread_t threadpool[WORKER_COUNT];
    pthread_mutex_t lock1;
    pthread_cond_t cond1;
} Pool;

Queue func_queue;
Pool TPool;

atomic_bool running = ATOMIC_VAR_INIT(true);
pthread_t workers[WORKER_COUNT];  // Added workers array declaration

void co_shutdown() {
    running = false;
    pthread_cond_broadcast(&func_queue.cond);
    for (int i = 0; i < WORKER_COUNT; i++) {
        pthread_join(workers[i], NULL);
    }
}

void task_queue_push(task_func_t func, void *arg) {
    pthread_mutex_lock(&func_queue.lock);
    task_t task = {func, arg};
    func_queue.Q[func_queue.tail] = task;
    func_queue.tail = (func_queue.tail + 1) % TASK_QUEUE_SIZE;
    pthread_cond_signal(&func_queue.cond);
    pthread_mutex_unlock(&func_queue.lock);
}

task_t task_queue_pop() {
    pthread_mutex_lock(&func_queue.lock);
    while (func_queue.head == func_queue.tail && running) {
        pthread_cond_wait(&func_queue.cond, &func_queue.lock);
    }
    task_t task = {0};
    if (running) {
        task = func_queue.Q[func_queue.head];
        func_queue.head = (func_queue.head + 1) % TASK_QUEUE_SIZE;
    }
    pthread_mutex_unlock(&func_queue.lock);
    return task;
}

void *worker_thread(void *arg) {
    int thread_id = *(int*)arg;
    char thread_name[32];
#ifdef __linux__
    sprintf(thread_name, "Worker-%d", thread_id);
    prctl(PR_SET_NAME, thread_name, 0, 0, 0);
#endif

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

    pthread_mutex_init(&func_queue.lock, NULL);
    pthread_cond_init(&func_queue.cond, NULL);
    func_queue.head = 0;
    func_queue.tail = 0;
    for (int i = 0; i < TASK_QUEUE_SIZE; i++) {
        func_queue.Q[i].func = NULL;
        func_queue.Q[i].arg = NULL;
    }

    TPool.head = 0;
    TPool.tail = 0;
    pthread_mutex_init(&TPool.lock1, NULL);
    pthread_cond_init(&TPool.cond1, NULL);

    for (int i = 0; i < WORKER_COUNT; i++) {
        int *thread_id = malloc(sizeof(int));
        *thread_id = i + 1;
        pthread_create(&workers[i], NULL, worker_thread, (void *)thread_id);
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
