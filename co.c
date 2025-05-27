#include "co.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>

#define SIZE 100

typedef struct {
    task_func_t func;
    void* arg;
} Task;

typedef struct {
    Task buffer[SIZE];
    int front;
    int rear;
    int count;
    pthread_mutex_t lock;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
} TaskQueue;

static TaskQueue tasks = {
        .front = 0, .rear = 0, .count = 0,
        .lock = PTHREAD_MUTEX_INITIALIZER,
        .not_empty = PTHREAD_COND_INITIALIZER,
        .not_full = PTHREAD_COND_INITIALIZER
};

static pthread_t threads[WORKER_COUNT];

// Queue Functions
int isEmpty(TaskQueue* q) {
    return q->count == 0;
}

int isFull(TaskQueue* q) {
    return q->count == SIZE;
}

void enqueue(TaskQueue* q, Task task) {
    pthread_mutex_lock(&q->lock);
    while (isFull(q))
        pthread_cond_wait(&q->not_full, &q->lock);
    q->buffer[q->rear] = task;
    q->rear = (q->rear + 1) % SIZE;
    q->count++;
    pthread_cond_signal(&q->not_empty);
    pthread_mutex_unlock(&q->lock);
}

Task dequeue(TaskQueue* q) {
    pthread_mutex_lock(&q->lock);
    while (isEmpty(q))
        pthread_cond_wait(&q->not_empty, &q->lock);
    Task task = q->buffer[q->front];
    q->front = (q->front + 1) % SIZE;
    q->count--;
    pthread_cond_signal(&q->not_full);
    pthread_mutex_unlock(&q->lock);
    return task;
}
//////////////////////////////////

void* worker(void* arg) {
    while (1) {
        Task task = dequeue(&tasks);
        if (task.func == NULL) break; // Shutdown sig
        task.func(task.arg);
    }
    return NULL;
}

void co_init() {
    for (int i = 0; i < WORKER_COUNT; i++) {
        pthread_create(&threads[i], NULL, worker, NULL);
    }
}

void co(task_func_t func, void *arg) {
    Task task = { .func = func, .arg = arg };
    enqueue(&tasks, task);
}

void co_shutdown() {
    for (int i = 0; i < WORKER_COUNT; i++) {
        Task poison = { .func = NULL, .arg = NULL };
        enqueue(&tasks, poison);
    }
    for (int i = 0; i < WORKER_COUNT; i++) {
        pthread_join(threads[i], NULL);
    }
}

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
