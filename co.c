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
#include <signal.h>
#endif

// Thread pool structure
typedef struct {
    pthread_t workers[WORKER_COUNT];       // Worker threads
    task_t task_queue[TASK_QUEUE_SIZE];    // Circular task queue
    int queue_front;                       // Front of the queue
    int queue_rear;                        // Rear of the queue
    int queue_count;                       // Number of tasks in queue
    pthread_mutex_t queue_mutex;           // Mutex for queue access
    pthread_cond_t task_available;         // Condition variable for tasks
    pthread_cond_t task_queue_not_full;    // Condition variable for queue space
    _Atomic bool shutdown;                 // Shutdown flag
} thread_pool_t;

// Global thread pool instance
static thread_pool_t *pool = NULL;

// Worker thread function
static void *worker_thread(void *arg) {
    (void)arg; // Unused
    char thread_name[32];
#ifdef __linux__
    prctl(PR_GET_NAME, thread_name, 0, 0, 0);
#else
    snprintf(thread_name, sizeof(thread_name), "worker");
#endif
    printf("[%s] Worker thread started\n", thread_name);

    while (1) {
        task_t task;
        bool have_task = false;

        // Lock the queue
        pthread_mutex_lock(&pool->queue_mutex);

        // Wait for a task or shutdown
        while (pool->queue_count == 0 && !atomic_load(&pool->shutdown)) {
            pthread_cond_wait(&pool->task_available, &pool->queue_mutex);
        }

        // Check for shutdown
        if (atomic_load(&pool->shutdown) && pool->queue_count == 0) {
            pthread_mutex_unlock(&pool->queue_mutex);
            break;
        }

        // Dequeue a task
        if (pool->queue_count > 0) {
            task = pool->task_queue[pool->queue_front];
            pool->queue_front = (pool->queue_front + 1) % TASK_QUEUE_SIZE;
            pool->queue_count--;
            have_task = true;

            // Signal that the queue is not full
            pthread_cond_signal(&pool->task_queue_not_full);
        }

        // Unlock the queue
        pthread_mutex_unlock(&pool->queue_mutex);

        // Execute the task
        if (have_task) {
            task.func(task.arg);
        }
    }

    printf("[%s] Worker thread exiting\n", thread_name);
    return NULL;
}

void co_init() {
    // Allocate thread pool
    pool = (thread_pool_t *)malloc(sizeof(thread_pool_t));
    if (!pool) {
        fprintf(stderr, "Failed to allocate thread pool\n");
        exit(1);
    }

    // Initialize thread pool
    pool->queue_front = 0;
    pool->queue_rear = 0;
    pool->queue_count = 0;
    atomic_store(&pool->shutdown, false);

    // Initialize mutex and condition variables
    if (pthread_mutex_init(&pool->queue_mutex, NULL) != 0) {
        fprintf(stderr, "Failed to initialize mutex\n");
        free(pool);
        exit(1);
    }
    if (pthread_cond_init(&pool->task_available, NULL) != 0 ||
        pthread_cond_init(&pool->task_queue_not_full, NULL) != 0) {
        fprintf(stderr, "Failed to initialize condition variables\n");
        pthread_mutex_destroy(&pool->queue_mutex);
        free(pool);
        exit(1);
    }

    // Create worker threads
    for (int i = 0; i < WORKER_COUNT; i++) {
        if (pthread_create(&pool->workers[i], NULL, worker_thread, NULL) != 0) {
            fprintf(stderr, "Failed to create worker thread %d\n", i);
            // Cleanup previously created threads
            atomic_store(&pool->shutdown, true);
            pthread_cond_broadcast(&pool->task_available);
            for (int j = 0; j < i; j++) {
                pthread_join(pool->workers[j], NULL);
            }
            pthread_mutex_destroy(&pool->queue_mutex);
            pthread_cond_destroy(&pool->task_available);
            pthread_cond_destroy(&pool->task_queue_not_full);
            free(pool);
            exit(1);
        }
    }

    printf("Thread pool initialized with %d workers\n", WORKER_COUNT);
}

void co(task_func_t func, void *arg) {
    if (!pool) {
        fprintf(stderr, "Thread pool not initialized\n");
        return;
    }

    // Create task
    task_t task = { .func = func, .arg = arg };

    // Lock the queue
    pthread_mutex_lock(&pool->queue_mutex);

    // Wait if the queue is full
    while (pool->queue_count == TASK_QUEUE_SIZE && !atomic_load(&pool->shutdown)) {
        pthread_cond_wait(&pool->task_queue_not_full, &pool->queue_mutex);
    }

    // Check for shutdown
    if (atomic_load(&pool->shutdown)) {
        pthread_mutex_unlock(&pool->queue_mutex);
        return;
    }

    // Enqueue the task
    pool->task_queue[pool->queue_rear] = task;
    pool->queue_rear = (pool->queue_rear + 1) % TASK_QUEUE_SIZE;
    pool->queue_count++;

    // Signal that a task is available
    pthread_cond_signal(&pool->task_available);

    // Unlock the queue
    pthread_mutex_unlock(&pool->queue_mutex);
}

void co_shutdown() {
    if (!pool) {
        return;
    }

    // Set shutdown flag
    atomic_store(&pool->shutdown, true);

    // Signal all workers to wake up
    pthread_mutex_lock(&pool->queue_mutex);
    pthread_cond_broadcast(&pool->task_available);
    pthread_cond_broadcast(&pool->task_queue_not_full);
    pthread_mutex_unlock(&pool->queue_mutex);

    // Join all worker threads
    for (int i = 0; i < WORKER_COUNT; i++) {
        pthread_join(pool->workers[i], NULL);
    }

    // Clean up resources
    pthread_mutex_destroy(&pool->queue_mutex);
    pthread_cond_destroy(&pool->task_available);
    pthread_cond_destroy(&pool->task_queue_not_full);
    free(pool);
    pool = NULL;

    printf("Thread pool shut down\n");
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