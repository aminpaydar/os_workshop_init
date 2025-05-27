// mohammad sadegh heydari 40213011
// raeen askari 40213019


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

pthread_t threads[32];
task_t tasks[100];
int task_num = 0;
int front = 0;
int rear = 0;
volatile sig_atomic_t running = 1;
pthread_mutex_t lock;

void *co_worker(void *arg) {
    while (running) {
      pthread_mutex_lock(&lock);
        if (task_num > 0) {
            task_t task = tasks[front];
            front = (front + 1) % 100;
            task_num--;
            pthread_mutex_unlock(&lock);
            task.func(task.arg);
        } else {
            pthread_mutex_unlock(&lock);
            usleep(1000);
        }
    }
    return NULL;
}


void co_init() {
    pthread_mutex_init(&lock, NULL);
    for(int i = 0; i < WORKER_COUNT; i++) {
      pthread_create(&threads[i], NULL, co_worker, NULL);
    }
}

void co_shutdown() {
    running = 0;
    for (int i = 0; i < WORKER_COUNT; i++) {
        pthread_join(threads[i], NULL);
    }
    printf("All worker threads shut down.\n");
    pthread_mutex_destroy(&lock);
}

void co(task_func_t func, void *arg) {
    pthread_mutex_lock(&lock);
    task_t task = {func, arg};
    tasks[rear] = task;
    rear = (rear + 1) % 100;
    task_num++;
    pthread_mutex_unlock(&lock);
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