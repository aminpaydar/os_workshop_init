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
    task_t tasks[TASK_QUEUE_SIZE];
    int front, rear, count;
    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
    bool shutdown;
} task_queue_t;

static pthread_t workers[WORKER_COUNT];
static task_queue_t task_queue;
static task_queue_t waiting_queue;  // New waiting queue

// Helper: enqueue task to a queue
void enqueue_task(task_queue_t *queue, task_t task) {
    queue->tasks[queue->rear] = task;
    queue->rear = (queue->rear + 1) % TASK_QUEUE_SIZE;
    queue->count++;
}

// Helper: dequeue task from a queue
task_t dequeue_task(task_queue_t *queue) {
    task_t task = queue->tasks[queue->front];
    queue->front = (queue->front + 1) % TASK_QUEUE_SIZE;
    queue->count--;
    return task;
}

void *worker_thread(void *arg) {
    int thread_num = (int)(long)arg;

    while (true) {
        pthread_mutex_lock(&task_queue.mutex);

        // Wait while both queues empty and no shutdown
        while (waiting_queue.count == 0 && task_queue.count == 0 && !task_queue.shutdown) {
            pthread_cond_wait(&task_queue.not_empty, &task_queue.mutex);
        }

        if (task_queue.shutdown) {
            pthread_mutex_unlock(&task_queue.mutex);
            break;
        }

        task_t task;

        // Priority: waiting queue first
        if (waiting_queue.count > 0) {
            task = dequeue_task(&waiting_queue);
            pthread_cond_signal(&waiting_queue.not_full);
        } else if (task_queue.count > 0) {
            task = dequeue_task(&task_queue);
            pthread_cond_signal(&task_queue.not_full);
        } else {
            // No task to process, continue loop (shouldn't happen because of the wait above)
            pthread_mutex_unlock(&task_queue.mutex);
            continue;
        }

        pthread_mutex_unlock(&task_queue.mutex);

        printf("Thread %d processing task\n", thread_num);
        task.func(task.arg);
    }

    return NULL;
}

void co_init() {
    // Initialize task queue
    task_queue.front = 0;
    task_queue.rear = 0;
    task_queue.count = 0;
    task_queue.shutdown = false;
    pthread_mutex_init(&task_queue.mutex, NULL);
    pthread_cond_init(&task_queue.not_empty, NULL);
    pthread_cond_init(&task_queue.not_full, NULL);

    // Initialize waiting queue
    waiting_queue.front = 0;
    waiting_queue.rear = 0;
    waiting_queue.count = 0;
    waiting_queue.shutdown = false;
    pthread_mutex_init(&waiting_queue.mutex, NULL);
    pthread_cond_init(&waiting_queue.not_empty, NULL);
    pthread_cond_init(&waiting_queue.not_full, NULL);

    for (int i = 0; i < WORKER_COUNT; i++) {
        pthread_create(&workers[i], NULL, worker_thread, (void *)(long)i);
    }
}

void co_shutdown() {
    pthread_mutex_lock(&task_queue.mutex);
    task_queue.shutdown = true;
    pthread_cond_broadcast(&task_queue.not_empty);
    pthread_mutex_unlock(&task_queue.mutex);

    pthread_mutex_lock(&waiting_queue.mutex);
    waiting_queue.shutdown = true;
    pthread_cond_broadcast(&waiting_queue.not_empty);
    pthread_mutex_unlock(&waiting_queue.mutex);

    for (int i = 0; i < WORKER_COUNT; i++) {
        pthread_join(workers[i], NULL);
    }

    pthread_mutex_destroy(&task_queue.mutex);
    pthread_cond_destroy(&task_queue.not_empty);
    pthread_cond_destroy(&task_queue.not_full);

    pthread_mutex_destroy(&waiting_queue.mutex);
    pthread_cond_destroy(&waiting_queue.not_empty);
    pthread_cond_destroy(&waiting_queue.not_full);
}

void co(task_func_t func, void *arg) {
    pthread_mutex_lock(&task_queue.mutex);

    if (task_queue.count == TASK_QUEUE_SIZE) {
        // Task queue full, push to waiting queue instead
        pthread_mutex_lock(&waiting_queue.mutex);
        while (waiting_queue.count == TASK_QUEUE_SIZE) {
            pthread_cond_wait(&waiting_queue.not_full, &waiting_queue.mutex);
        }

        task_t t = { func, arg };
        enqueue_task(&waiting_queue, t);

        pthread_cond_signal(&waiting_queue.not_empty);
        pthread_mutex_unlock(&waiting_queue.mutex);
    } else {
        // Task queue has space, push task here
        while (task_queue.count == TASK_QUEUE_SIZE) {
            pthread_cond_wait(&task_queue.not_full, &task_queue.mutex);
        }

        task_t t = { func, arg };
        enqueue_task(&task_queue, t);

        pthread_cond_signal(&task_queue.not_empty);
    }

    pthread_mutex_unlock(&task_queue.mutex);
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
