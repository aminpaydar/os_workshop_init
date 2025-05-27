#include "co.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdatomic.h>
#include <signal.h>

#ifdef linux
#include <sys/prctl.h>
#endif

typedef struct {
    task_t queue[TASK_QUEUE_SIZE];
    int front;
    int rear;
    pthread_mutex_t lock;
    pthread_cond_t not_empty;
} task_queue_t;

static task_queue_t task_q = {
    .front = 0,
    .rear = 0,
    .lock = PTHREAD_MUTEX_INITIALIZER,
    .not_empty = PTHREAD_COND_INITIALIZER
};

static pthread_t worker_threads[WORKER_COUNT];
static atomic_bool is_active = true;
static atomic_bool initialized = false;

static void enqueue_task(task_func_t func, void *arg) {
    pthread_mutex_lock(&task_q.lock);
    task_q.queue[task_q.rear].func = func;
    task_q.queue[task_q.rear].arg = arg;
    task_q.rear = (task_q.rear + 1) % TASK_QUEUE_SIZE;
    pthread_cond_signal(&task_q.not_empty);
    pthread_mutex_unlock(&task_q.lock);
}

static task_t dequeue_task() {
    pthread_mutex_lock(&task_q.lock);
    while (task_q.front == task_q.rear && is_active) {
        pthread_cond_wait(&task_q.not_empty, &task_q.lock);
    }
    task_t task = task_q.queue[task_q.front];
    task_q.front = (task_q.front + 1) % TASK_QUEUE_SIZE;
    pthread_mutex_unlock(&task_q.lock);
    return task;
}

static void* worker_loop(void *arg) {
    int id = *(int*)arg;
    char name[32];
    snprintf(name, sizeof(name), "T-%02d", id);
#ifdef linux
    prctl(PR_SET_NAME, name, 0, 0, 0);
#endif
    free(arg);

    while (is_active) {
        task_t task = dequeue_task();
        if (task.func)
            task.func(task.arg);
    }

    return NULL;
}

void co_init() {
    if (initialized) return;
    for (int i = 0; i < WORKER_COUNT; ++i) {
        int *id = malloc(sizeof(int));
        *id = i + 1;
        pthread_create(&worker_threads[i], NULL, worker_loop, id);
    }
    initialized = true;
}

void co_shutdown() {
    is_active = false;
    pthread_cond_broadcast(&task_q.not_empty);
    for (int i = 0; i < WORKER_COUNT; ++i) {
        pthread_join(worker_threads[i], NULL);
    }
}

void co(task_func_t func, void *arg) {
    enqueue_task(func, arg);
}

int wait_sig() {
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGTERM);
    pthread_sigmask(SIG_BLOCK, &mask, NULL);

    printf("Waiting for Ctrl+C or SIGTERM...\n");
    int sig;
    sigwait(&mask, &sig);
    printf("Caught signal %d, terminating...\n", sig);
    return sig;
}
