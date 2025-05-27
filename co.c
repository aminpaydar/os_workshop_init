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
//Hirad Harirchi 40113410
//Sarvenaz Haji Mohammad Reza 40112012

// Thread pool
static pthread_t worker_threads[WORKER_COUNT];

// Task queue
static task_t task_queue[TASK_QUEUE_SIZE];
static int queue_head = 0;
static int queue_tail = 0;
static int queue_size = 0;

// Synchronization
static pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t queue_not_empty = PTHREAD_COND_INITIALIZER;
static pthread_cond_t queue_not_full = PTHREAD_COND_INITIALIZER;
static atomic_bool running = true;

static void* worker_function(void* arg) {
    
    while (atomic_load(&running)) {
        pthread_mutex_lock(&queue_mutex);
        
        while (queue_size == 0 && atomic_load(&running)) {
            pthread_cond_wait(&queue_not_empty, &queue_mutex);
        }
        
        // Check if we're shutting down and there are no more tasks
        if (queue_size == 0 && !atomic_load(&running)) {
            pthread_mutex_unlock(&queue_mutex);
            break;
        }
        
        task_t task = task_queue[queue_head];
        queue_head = (queue_head + 1) % TASK_QUEUE_SIZE;
        queue_size--;
        
        pthread_cond_signal(&queue_not_full);
        pthread_mutex_unlock(&queue_mutex);
        
        // Execute the task
        if (task.func) {
            task.func(task.arg);
        }
    }
    
    return NULL;
}

void co_init() {
    atomic_store(&running, true);
    
    for (int i = 0; i < WORKER_COUNT; i++) {
        if (pthread_create(&worker_threads[i], NULL, worker_function, NULL) != 0) {
            perror("Failed to create worker thread");
            exit(EXIT_FAILURE);
        }
    }
}

void co_shutdown() {
    // Signal that we're shutting down
    atomic_store(&running, false);
    
    // Wake up all worker threads
    pthread_mutex_lock(&queue_mutex);
    pthread_cond_broadcast(&queue_not_empty);
    pthread_mutex_unlock(&queue_mutex);
    
    // Wait for all worker threads to finish
    for (int i = 0; i < WORKER_COUNT; i++) {
        pthread_join(worker_threads[i], NULL);
    }
    
    // Clean up synchronization primitives
    pthread_mutex_destroy(&queue_mutex);
    pthread_cond_destroy(&queue_not_empty);
    pthread_cond_destroy(&queue_not_full);
}

void co(task_func_t func, void *arg) {
    // Create a task
    task_t task = {
        .func = func,
        .arg = arg
    };
    
    // Add the task to the queue
    pthread_mutex_lock(&queue_mutex);
    
    // Wait if the queue is full
    while (queue_size == TASK_QUEUE_SIZE && atomic_load(&running)) {
        pthread_cond_wait(&queue_not_full, &queue_mutex);
    }
    
    // Check if we're still running
    if (!atomic_load(&running)) {
        pthread_mutex_unlock(&queue_mutex);
        return;
    }
    
    // Add the task to the queue
    task_queue[queue_tail] = task;
    queue_tail = (queue_tail + 1) % TASK_QUEUE_SIZE;
    queue_size++;
    
    // Signal that the queue is not empty
    pthread_cond_signal(&queue_not_empty);
    pthread_mutex_unlock(&queue_mutex);
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