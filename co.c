#include "co.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdatomic.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#ifdef __linux__
#include <sys/prctl.h>
#elif defined(__APPLE__)
#include <pthread.h>
#endif

typedef struct {
    task_t tasks[TASK_QUEUE_SIZE];
    int head, tail;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
} task_queue_t;

task_queue_t task_queue = {.head = 0, .tail = 0, .mutex = PTHREAD_MUTEX_INITIALIZER, .cond = PTHREAD_COND_INITIALIZER};
pthread_t workers[WORKER_COUNT];
atomic_bool running = ATOMIC_VAR_INIT(true);
atomic_flag workers_init_flag = ATOMIC_FLAG_INIT;

void task_queue_push(task_func_t func, void *arg) {
    pthread_mutex_lock(&task_queue.mutex);
    // Wait for space in the queue
    while ((task_queue.tail + 1) % TASK_QUEUE_SIZE == task_queue.head) {
        pthread_cond_wait(&task_queue.cond, &task_queue.mutex);
    }
    task_t task = {.func = func, .arg = arg};
    task_queue.tasks[task_queue.tail] = task;
    task_queue.tail = (task_queue.tail + 1) % TASK_QUEUE_SIZE;
    pthread_cond_signal(&task_queue.cond);
    pthread_mutex_unlock(&task_queue.mutex);
}

task_t task_queue_pop() {
    pthread_mutex_lock(&task_queue.mutex);
    while (task_queue.head == task_queue.tail && atomic_load(&running)) {
        pthread_cond_wait(&task_queue.cond, &task_queue.mutex);
    }
    
    task_t task = {NULL, NULL};
    if (task_queue.head != task_queue.tail) {
        task = task_queue.tasks[task_queue.head];
        task_queue.head = (task_queue.head + 1) % TASK_QUEUE_SIZE;
    }
    pthread_mutex_unlock(&task_queue.mutex);
    return task;
}

void* worker_thread(void* arg) {
    int thread_id = *(int*)arg;
    free(arg);
    
    char thread_name[32];
    snprintf(thread_name, sizeof(thread_name), "Worker-%d", thread_id);
    
    #ifdef __linux__
    prctl(PR_SET_NAME, thread_name);
    #elif defined(__APPLE__)
    pthread_setname_np(thread_name);
    #endif

    while (atomic_load(&running)) {
        task_t task = task_queue_pop();
        if (!atomic_load(&running)) break;
        if (task.func) {
            task.func(task.arg);
        }
    }
    return NULL;
}

void co_init() {
    if (atomic_flag_test_and_set(&workers_init_flag)) return;

    for (int i = 0; i < WORKER_COUNT; i++) {
        int* thread_id = malloc(sizeof(int));
        if (!thread_id) {
            perror("Failed to allocate thread ID");
            exit(EXIT_FAILURE);
        }
        *thread_id = i + 1;
        
        int err = pthread_create(&workers[i], NULL, worker_thread, thread_id);
        if (err != 0) {
            free(thread_id);
            fprintf(stderr, "Failed to create thread: %s\n", strerror(err));
            exit(EXIT_FAILURE);
        }
    }
}

void co_shutdown() {
    atomic_store(&running, false);
    pthread_mutex_lock(&task_queue.mutex);
    pthread_cond_broadcast(&task_queue.cond);
    pthread_mutex_unlock(&task_queue.mutex);
    
    for (int i = 0; i < WORKER_COUNT; i++) {
        pthread_join(workers[i], NULL);
    }
    
    pthread_mutex_destroy(&task_queue.mutex);
    pthread_cond_destroy(&task_queue.cond);
    atomic_flag_clear(&workers_init_flag);
}

void co(task_func_t func, void* arg) {
    task_queue_push(func, arg);
}

int wait_sig() {
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGTERM);
    pthread_sigmask(SIG_BLOCK, &mask, NULL);
    
    printf("Waiting for SIGINT (Ctrl+C) or SIGTERM...\n");
    int signum;
    do {
        int result = sigwait(&mask, &signum);
        if (result != 0 && errno != EINTR) {
            perror("sigwait failed");
            exit(EXIT_FAILURE);
        }
    } while (errno == EINTR);
    
    printf("Signal %d received. Shutting down...\n", signum);
    return signum;
}