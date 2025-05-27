#include "co.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdatomic.h>
#include <signal.h>
#ifdef __linux__
#include <sys/prctl.h>
//#include <bits/types/sigset_t.h>
//#include <bits/sigaction.h>
#else
// macOS includes
#include <signal.h>
#endif

<<<<<<< HEAD
static pthread_t workers[WORKER_COUNT];
static task_t task_queue[TASK_QUEUE_SIZE];

static int task_count = 0;
static int task_head = 0;
static int task_tail = 0;

static pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t queue_cond = PTHREAD_COND_INITIALIZER;
static bool shutdown_flag = false;


void* worker_thread(void* arg) {
    while (1) {
        pthread_mutex_lock(&queue_mutex);

        while (task_count == 0 && !shutdown_flag) {
            pthread_cond_wait(&queue_cond, &queue_mutex);
        }

        if (shutdown_flag && task_count == 0) {
            pthread_mutex_unlock(&queue_mutex);
            break;
        }

        task_t task = task_queue[task_head];
        task_head = (task_head + 1) % TASK_QUEUE_SIZE;
        task_count--;

        pthread_mutex_unlock(&queue_mutex);

        task.func(task.arg);
    }

=======
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
>>>>>>> af2f994cb62b1860ab434921740488c751f16a58
    return NULL;
}

void co_init() {
<<<<<<< HEAD
    for (int i = 0; i < WORKER_COUNT; i++) {
        pthread_create(&workers[i], NULL, worker_thread, NULL);
    }


}

void co_shutdown() {
    pthread_mutex_lock(&queue_mutex);
    shutdown_flag = true;
    pthread_cond_broadcast(&queue_cond);
    pthread_mutex_unlock(&queue_mutex);

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
>>>>>>> af2f994cb62b1860ab434921740488c751f16a58
    for (int i = 0; i < WORKER_COUNT; i++) {
        pthread_join(workers[i], NULL);
    }
}

void co(task_func_t func, void *arg) {
<<<<<<< HEAD
    pthread_mutex_lock(&queue_mutex);

    if (task_count < TASK_QUEUE_SIZE) {
        task_queue[task_tail].func = func;
        task_queue[task_tail].arg = arg;
        task_tail = (task_tail + 1) % TASK_QUEUE_SIZE;
        task_count++;

        pthread_cond_signal(&queue_cond);
    } else {
        fprintf(stderr, "Task queue is full!\n");
    }

    pthread_mutex_unlock(&queue_mutex);
=======
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