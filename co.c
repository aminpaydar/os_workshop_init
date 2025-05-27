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
// alireza dehnavi 40113412

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

typedef struct {
    pthread_t threads[WORKER_COUNT];
    task_queue_t queue;
} co_context_t;


static co_context_t *co_ctx = NULL;

static void *worker_thread(void *arg) {
    task_queue_t *queue = (task_queue_t *)arg;

    while (atomic_load(&queue->running)) {
        task_t task;

        
        pthread_mutex_lock(&queue->mutex);

        
        while (queue->count == 0 && atomic_load(&queue->running)) {
            pthread_cond_wait(&queue->not_empty, &queue->mutex);
        }

        
        if (!atomic_load(&queue->running)) {
            pthread_mutex_unlock(&queue->mutex);
            break;
        }

        
        task = queue->tasks[queue->head];
        queue->head = (queue->head + 1) % TASK_QUEUE_SIZE;
        queue->count--;

        
        pthread_cond_signal(&queue->not_full);
        pthread_mutex_unlock(&queue->mutex);

        
        task.func(task.arg);
    }

    return NULL;
}


typedef struct {
    task_t tasks[TASK_QUEUE_SIZE];
    int head, tail;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
} task_queue_t;

task_queue_t task_queue = {.head = 0, .tail = 0, .mutex = PTHREAD_MUTEX_INITIALIZER, .cond = PTHREAD_COND_INITIALIZER};
pthread_t workers[WORKER_COUNT];
atomic_bool running = true;
atomic_bool workers_init = false;

void task_queue_push(task_func_t func, void *arg) {
    pthread_mutex_lock(&task_queue.mutex);
    task_t task = {func, arg};
    task_queue.tasks[task_queue.tail] = task;
    task_queue.tail = (task_queue.tail + 1) % TASK_QUEUE_SIZE;
    pthread_cond_signal(&task_queue.cond);
    pthread_mutex_unlock(&task_queue.mutex);
}

task_t task_queue_pop() {
    pthread_mutex_lock(&task_queue.mutex);
    while (task_queue.head == task_queue.tail && running) {
        pthread_cond_wait(&task_queue.cond, &task_queue.mutex);
    }
    task_t task = task_queue.tasks[task_queue.head];
    task_queue.head = (task_queue.head + 1) % TASK_QUEUE_SIZE;
    pthread_mutex_unlock(&task_queue.mutex);
    return task;
}

void *worker_thread(void *arg) {
    int thread_id = *(int*) arg;
    char thread_name[32];
    sprintf(thread_name, "Worker-%d", thread_id);
    #ifdef __linux__
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
<<<<<<< HEAD
    // TO BE IMPLEMENTED
    co_ctx = (co_context_t *)malloc(sizeof(co_context_t));
    if (!co_ctx) {
        fprintf(stderr, "Failed to allocate co_context\n");
        exit(1);
    }

    co_ctx->queue.head = 0;
    co_ctx->queue.tail = 0;
    co_ctx->queue.count = 0;
    atomic_store(&co_ctx->queue.running, true);

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

    for (int i = 0; i < WORKER_COUNT; i++) {
        if (pthread_create(&co_ctx->threads[i], NULL, worker_thread, &co_ctx->queue) != 0) {
            fprintf(stderr, "Failed to create worker thread %d\n", i);
            co_shutdown();
            exit(1);
        }
    }
}

void co_shutdown() {
    // TO BE IMPLEMENTED
        if (!co_ctx) return;

    atomic_store(&co_ctx->queue.running, false);

    pthread_mutex_lock(&co_ctx->queue.mutex);
    pthread_cond_broadcast(&co_ctx->queue.not_empty);
    pthread_cond_broadcast(&co_ctx->queue.not_full);
    pthread_mutex_unlock(&co_ctx->queue.mutex);

    for (int i = 0; i < WORKER_COUNT; i++) {
        pthread_join(co_ctx->threads[i], NULL);
    }

    pthread_mutex_destroy(&co_ctx->queue.mutex);
    pthread_cond_destroy(&co_ctx->queue.not_empty);
    pthread_cond_destroy(&co_ctx->queue.not_full);

    free(co_ctx);
    co_ctx = NULL;
}

void co(task_func_t func, void *arg) {
    // TO BE IMPLEMENTED
        if (!co_ctx || !atomic_load(&co_ctx->queue.running)) {
        fprintf(stderr, "Coroutine system not initialized or shutting down\n");
        return;
    }

    task_t task = {func, arg};
    pthread_mutex_lock(&co_ctx->queue.mutex);

    while (co_ctx->queue.count == TASK_QUEUE_SIZE && atomic_load(&co_ctx->queue.running)) {
        pthread_cond_wait(&co_ctx->queue.not_full, &co_ctx->queue.mutex);
    }

    if (!atomic_load(&co_ctx->queue.running)) {
        pthread_mutex_unlock(&co_ctx->queue.mutex);
        return;
    }

    co_ctx->queue.tasks[co_ctx->queue.tail] = task;
    co_ctx->queue.tail = (co_ctx->queue.tail + 1) % TASK_QUEUE_SIZE;
    co_ctx->queue.count++;

    pthread_cond_signal(&co_ctx->queue.not_empty);
    pthread_mutex_unlock(&co_ctx->queue.mutex);
=======
    if (workers_init) return;
    int worker_ids[WORKER_COUNT];
    for (int i = 0; i < WORKER_COUNT; i++) {
        worker_ids[i] = i + 1;
    }
    for (int i = 0; i < WORKER_COUNT; i++) {
        int *thread_id = malloc(sizeof(int));
        *thread_id = i + 1;
        pthread_create(&workers[i], NULL, worker_thread, (void*) thread_id);
    }

    workers_init = true;
}

void co_shutdown() {
    running = false;
    pthread_cond_broadcast(&task_queue.cond);
    for (int i = 0; i < WORKER_COUNT; i++) {
        pthread_join(workers[i], NULL);
    }
}

void co(task_func_t func, void *arg) {
    task_queue_push(func, arg);
>>>>>>> af2f994cb62b1860ab434921740488c751f16a58
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