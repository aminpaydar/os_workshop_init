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
    pthread_t* threads;
    task_t* task_queue;
    int max_threads;
    int queue_size;
    int task_count;
    int head;
    int tail;
    pthread_mutex_t lock;
    pthread_cond_t cond;
    _Atomic bool shutdown;
} ThreadPool;

static ThreadPool* pool = NULL;

static void* worker_thread(void* arg) {
    ThreadPool* pool = (ThreadPool*)arg;
    
    while (true) {
        pthread_mutex_lock(&pool->lock);
        
        while (pool->task_count == 0 && !atomic_load(&pool->shutdown)) {
            pthread_cond_wait(&pool->cond, &pool->lock);
        }
        
        if (atomic_load(&pool->shutdown) && pool->task_count == 0) {
            pthread_mutex_unlock(&pool->lock);
            return NULL;
        }
        
        task_t task = pool->task_queue[pool->head];
        pool->head = (pool->head + 1) % pool->queue_size;
        pool->task_count--;
        
        pthread_mutex_unlock(&pool->lock);
        
        task.func(task.arg);
    }
    
    return NULL;
}

void co_init() {
    pool = malloc(sizeof(ThreadPool));
    if (!pool) {
        fprintf(stderr, "Failed to allocate thread pool\n");
        exit(1);
    }
    
    pool->max_threads = WORKER_COUNT;
    pool->queue_size = TASK_QUEUE_SIZE;
    pool->task_count = 0;
    pool->head = 0;
    pool->tail = 0;
    atomic_store(&pool->shutdown, false);
    
    if (pthread_mutex_init(&pool->lock, NULL) != 0) {
        free(pool);
        exit(1);
    }
    if (pthread_cond_init(&pool->cond, NULL) != 0) {
        pthread_mutex_destroy(&pool->lock);
        free(pool);
        exit(1);
    }
    
    pool->threads = malloc(sizeof(pthread_t) * pool->max_threads);
    pool->task_queue = malloc(sizeof(task_t) * pool->queue_size);
    
    if (!pool->threads || !pool->task_queue) {
        free(pool->threads);
        free(pool->task_queue);
        pthread_mutex_destroy(&pool->lock);
        pthread_cond_destroy(&pool->cond);
        free(pool);
        exit(1);
    }
    
    for (int i = 0; i < pool->max_threads; i++) {
        if (pthread_create(&pool->threads[i], NULL, worker_thread, pool) != 0) {
            pthread_mutex_destroy(&pool->lock);
            pthread_cond_destroy(&pool->cond);
            free(pool->threads);
            free(pool->task_queue);
            free(pool);
            exit(1);
        }
    }
}

void co_shutdown() {
    if (!pool) return;
    
    
    atomic_store(&pool->shutdown, true);
    
    pthread_mutex_lock(&pool->lock);
    pthread_cond_broadcast(&pool->cond);
    pthread_mutex_unlock(&pool->lock);
    
    for (int i = 0; i < pool->max_threads; i++) {
        pthread_join(pool->threads[i], NULL);
    }
    
    pthread_mutex_destroy(&pool->lock);
    pthread_cond_destroy(&pool->cond);
    free(pool->threads);
    free(pool->task_queue);
    free(pool);
    pool = NULL;
}

void co(task_func_t func, void *arg) {
    if (!pool) {
        fprintf(stderr, "Thread pool not initialized\n");
        return;
    }
    
    pthread_mutex_lock(&pool->lock);
    
    if (pool->task_count == pool->queue_size) {
        pthread_mutex_unlock(&pool->lock);
        fprintf(stderr, "Task queue full\n");
        return;
    }
    
    pool->task_queue[pool->tail].func = func;
    pool->task_queue[pool->tail].arg = arg;
    pool->tail = (pool->tail + 1) % pool->queue_size;
    pool->task_count++;
    
    pthread_cond_signal(&pool->cond);
    pthread_mutex_unlock(&pool->lock);
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
