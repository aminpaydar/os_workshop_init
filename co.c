// Ali Ahmadi Esfidi 
// Fereshreh Mokhtarzadeh


#include "co.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdatomic.h>
#include <signal.h>
#ifdef linux
#include <sys/prctl.h>
#include <bits/types/sigset_t.h>
#include <bits/sigaction.h>
#else
#include <signal.h>
#endif

// Task queue structure
typedef struct {
    task_t tasks[TASK_QUEUE_SIZE];
    int head;
    int tail;
    int count;
    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
    _Atomic bool running;
} task_queue_t;

// Worker thread context
typedef struct {
    pthread_t threads[WORKER_COUNT];
    task_queue_t queue;
} co_context_t;

// Global context
static co_context_t *co_ctx = NULL;

// Worker thread function
static void *worker_thread(void *arg) {
    task_queue_t *queue = (task_queue_t *)arg;

    while (atomic_load(&queue->running)) {
        task_t task;

        // Lock the queue
        pthread_mutex_lock(&queue->mutex);

        // Wait for tasks if queue is empty
        while (queue->count == 0 && atomic_load(&queue->running)) {
            pthread_cond_wait(&queue->not_empty, &queue->mutex);
        }

        // Check if we should exit
        if (!atomic_load(&queue->running)) {
            pthread_mutex_unlock(&queue->mutex);
            break;
        }

        // Dequeue task
        task = queue->tasks[queue->head];
        queue->head = (queue->head + 1) % TASK_QUEUE_SIZE;
        queue->count--;

        // Signal that queue is not full
        pthread_cond_signal(&queue->not_full);
        pthread_mutex_unlock(&queue->mutex);

        // Execute task
        task.func(task.arg);
    }

    return NULL;
}

void co_init() {
    // Allocate context
    co_ctx = (co_context_t *)malloc(sizeof(co_context_t));
    if (!co_ctx) {
        fprintf(stderr, "Failed to allocate co_context\n");
        exit(1);
    }

    // Initialize queue
    co_ctx->queue.head = 0;
    co_ctx->queue.tail = 0;
    co_ctx->queue.count = 0;
    atomic_store(&co_ctx->queue.running, true);

    // Initialize synchronization primitives
    if (pthread_mutex_init(&co_ctx->queue.mutex, NULL) != 0) {
        fprintf(stderr, "Failed to initialize mutex\n");
        free(co_ctx);
        exit(1);
    }

    if (pthread_cond_init(&co_ctx->queue.not_empty, NULL) != 0 ||
        pthread_cond_init(&co_ctx->queue.not_full, NULL) != 0) {
        fprintf(stderr, "Failed to initialize condition variables\n");
        pthread_mutex_destroy(&co_ctx->queue.mutex);
        free(co_ctx);
        exit(1);
    }

    // Create worker threads
    for (int i = 0; i < WORKER_COUNT; i++) {
        if (pthread_create(&co_ctx->threads[i], NULL, worker_thread, &co_ctx->queue) != 0) {
            fprintf(stderr, "Failed to create worker thread %d\n", i);
            co_shutdown();
            exit(1);
        }
    }
}

void co_shutdown() {
    if (!co_ctx) return;

    // Signal workers to stop
    atomic_store(&co_ctx->queue.running, false);

    // Wake up all workers
    pthread_mutex_lock(&co_ctx->queue.mutex);
    pthread_cond_broadcast(&co_ctx->queue.not_empty);
    pthread_cond_broadcast(&co_ctx->queue.not_full);
    pthread_mutex_unlock(&co_ctx->queue.mutex);

    // Join all worker threads
    for (int i = 0; i < WORKER_COUNT; i++) {
        pthread_join(co_ctx->threads[i], NULL);
    }

    // Clean up synchronization primitives
    pthread_mutex_destroy(&co_ctx->queue.mutex);
    pthread_cond_destroy(&co_ctx->queue.not_empty);
    pthread_cond_destroy(&co_ctx->queue.not_full);

    // Free context
    free(co_ctx);
    co_ctx = NULL;
}

void co(task_func_t func, void *arg) {
    if (!co_ctx || !atomic_load(&co_ctx->queue.running)) {
        fprintf(stderr, "Coroutine system not initialized or shutting down\n");
        return;
    }

    // Create task
    task_t task = {func, arg};

    // Enqueue task
    pthread_mutex_lock(&co_ctx->queue.mutex);

    // Wait if queue is full
    while (co_ctx->queue.count == TASK_QUEUE_SIZE && atomic_load(&co_ctx->queue.running)) {
        pthread_cond_wait(&co_ctx->queue.not_full, &co_ctx->queue.mutex);
    }

    // Check if system is shutting down
    if (!atomic_load(&co_ctx->queue.running)) {
        pthread_mutex_unlock(&co_ctx->queue.mutex);
        return;
    }

    // Add task to queue
    co_ctx->queue.tasks[co_ctx->queue.tail] = task;
    co_ctx->queue.tail = (co_ctx->queue.tail + 1) % TASK_QUEUE_SIZE;
    co_ctx->queue.count++;

    // Signal that queue is not empty
    pthread_cond_signal(&co_ctx->queue.not_empty);
    pthread_mutex_unlock(&co_ctx->queue.mutex);
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