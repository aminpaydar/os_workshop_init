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
#include <pthread.h>
#include <signal.h>
#endif

#define THREAD_POOL_SIZE 32

typedef struct TaskNode {
    task_func_t func;
    void *arg;
    struct TaskNode *next;
} TaskNode;

typedef struct {
    TaskNode *front, *rear;
    pthread_mutex_t lock;
    pthread_cond_t cond;
    int shutdown;
} TaskQueue;

static TaskQueue task_queue;
static pthread_t thread_pool[THREAD_POOL_SIZE];

static void* worker_thread(void* arg) {
    (void)arg;
    while (1) {
        pthread_mutex_lock(&task_queue.lock);
        while (task_queue.front == NULL && !task_queue.shutdown) {
            pthread_cond_wait(&task_queue.cond, &task_queue.lock);
        }
        if (task_queue.shutdown && task_queue.front == NULL) {
            pthread_mutex_unlock(&task_queue.lock);
            break;
        }
        TaskNode *node = task_queue.front;
        if (node) {
            task_queue.front = node->next;
            if (task_queue.front == NULL) task_queue.rear = NULL;
        }
        pthread_mutex_unlock(&task_queue.lock);

        if (node) {
            node->func(node->arg);
            free(node);
        }
    }
    return NULL;
}

void co_init() {
    task_queue.front = task_queue.rear = NULL;
    pthread_mutex_init(&task_queue.lock, NULL);
    pthread_cond_init(&task_queue.cond, NULL);
    task_queue.shutdown = 0;
    for (int i = 0; i < THREAD_POOL_SIZE; ++i) {
        pthread_create(&thread_pool[i], NULL, worker_thread, NULL);
    }
}

void co_shutdown() {
    pthread_mutex_lock(&task_queue.lock);
    task_queue.shutdown = 1;
    pthread_cond_broadcast(&task_queue.cond);
    pthread_mutex_unlock(&task_queue.lock);
    for (int i = 0; i < THREAD_POOL_SIZE; ++i) {
        pthread_join(thread_pool[i], NULL);
    }
    while (task_queue.front) {
        TaskNode *tmp = task_queue.front;
        task_queue.front = tmp->next;
        free(tmp);
    }
    pthread_mutex_destroy(&task_queue.lock);
    pthread_cond_destroy(&task_queue.cond);
}

void co(task_func_t func, void *arg) {
    TaskNode *node = malloc(sizeof(TaskNode));
    node->func = func;
    node->arg = arg;
    node->next = NULL;

    pthread_mutex_lock(&task_queue.lock);
    if (task_queue.rear) {
        task_queue.rear->next = node;
        task_queue.rear = node;
    } else {
        task_queue.front = task_queue.rear = node;
    }
    pthread_cond_signal(&task_queue.cond);
    pthread_mutex_unlock(&task_queue.lock);
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