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
#include <pthread.h>

pthread_t workers[WORKER_COUNT];
task_t tasks[TASK_QUEUE_SIZE];
pthread_mutex_t mutex;
pthread_cond_t cond;
int head = 0;
int tail = 0;

void* work()
{
    while (true)
    {
        task_t task;
        pthread_mutex_lock(&mutex);
        while (head == tail)
        {
            pthread_cond_wait(&cond, &mutex);
        }
        task = tasks[head];
        head++;
        head %= TASK_QUEUE_SIZE;
        pthread_mutex_unlock(&mutex);
        task.func(task.arg);
    }
}

void co_init() {
    pthread_cond_init(&cond, NULL);
    for (size_t i = 0; i < WORKER_COUNT; i++)
    {
        pthread_create(&workers[i], NULL, work, NULL);
    }
    
}

void co_shutdown() {
    for (size_t i = 0; i < WORKER_COUNT; i++)
    {
        pthread_cancel(workers[i]);
    }
}

void co(task_func_t func, void *arg) {
    task_t task;
    task.func = func;
    task.arg = arg;
    pthread_mutex_lock(&mutex);
    tasks[tail] = task;
    tail++;
    tail %= TASK_QUEUE_SIZE;
    pthread_cond_signal(&cond);
    pthread_mutex_unlock(&mutex);
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