#include "co.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdatomic.h>
#include <signal.h>
#ifdef __linux__
#include <sys/prctl.h>
<<<<<<< HEAD
#include <bits/types/sigset_t.h>
#include <bits/sigaction.h>
#else
// macOS includes
#include <signal.h>
#endif

=======
#else
#include <signal.h>
#endif

// صف کار ساده با قفل و شرط
>>>>>>> 475d620 (adding code-saleh sabagh)
typedef struct {
    task_t tasks[TASK_QUEUE_SIZE];
    int head, tail;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
} task_queue_t;

<<<<<<< HEAD
task_queue_t task_queue = {.head = 0, .tail = 0, .mutex = PTHREAD_MUTEX_INITIALIZER, .cond = PTHREAD_COND_INITIALIZER};
pthread_t workers[WORKER_COUNT];
atomic_bool running = true;
atomic_bool workers_init = false;

void task_queue_push(task_func_t func, void *arg) {
    pthread_mutex_lock(&task_queue.mutex);
    task_t task = {func, arg};
    task_queue.tasks[task_queue.tail] = task;
    task_queue.tail = (task_queue.tail + 1) % TASK_QUEUE_SIZE;
=======
// متغیرهای سراسری
static task_queue_t task_queue = {
    .head = 0, .tail = 0,
    .mutex = PTHREAD_MUTEX_INITIALIZER,
    .cond = PTHREAD_COND_INITIALIZER
};

static pthread_t workers[WORKER_COUNT];
static atomic_bool running = true;
static atomic_bool workers_started = false;

// وارد کردن کار به صف
static void task_queue_push(task_func_t func, void *arg) {
    pthread_mutex_lock(&task_queue.mutex);

    // ساده‌ترین مدل: اگر صف پر شود، کار جدید جایگزین می‌شود (بافر دایره‌ای)
    task_queue.tasks[task_queue.tail].func = func;
    task_queue.tasks[task_queue.tail].arg = arg;
    task_queue.tail = (task_queue.tail + 1) % TASK_QUEUE_SIZE;

>>>>>>> 475d620 (adding code-saleh sabagh)
    pthread_cond_signal(&task_queue.cond);
    pthread_mutex_unlock(&task_queue.mutex);
}

<<<<<<< HEAD
task_t task_queue_pop() {
    pthread_mutex_lock(&task_queue.mutex);
    while (task_queue.head == task_queue.tail && running) {
        pthread_cond_wait(&task_queue.cond, &task_queue.mutex);
    }
    task_t task = task_queue.tasks[task_queue.head];
    task_queue.head = (task_queue.head + 1) % TASK_QUEUE_SIZE;
=======
// خارج کردن کار از صف
static task_t task_queue_pop() {
    pthread_mutex_lock(&task_queue.mutex);

    while (task_queue.head == task_queue.tail && running) {
        pthread_cond_wait(&task_queue.cond, &task_queue.mutex);
    }

    task_t task = {.func = NULL, .arg = NULL};
    if (task_queue.head != task_queue.tail) {
        task = task_queue.tasks[task_queue.head];
        task_queue.head = (task_queue.head + 1) % TASK_QUEUE_SIZE;
    }

>>>>>>> 475d620 (adding code-saleh sabagh)
    pthread_mutex_unlock(&task_queue.mutex);
    return task;
}

<<<<<<< HEAD
void *worker_thread(void *arg) {
    int thread_id = *(int*) arg;
    char thread_name[32];
    sprintf(thread_name, "Worker-%d", thread_id);
    #ifdef __linux__
    prctl(PR_SET_NAME, thread_name, 0, 0, 0);
    #endif
=======
static void *worker_thread(void *arg) {
    int thread_id = *(int *)arg;
    free(arg);

#ifdef __linux__
    char thread_name[32];
    snprintf(thread_name, sizeof(thread_name), "Worker-%d", thread_id);
    prctl(PR_SET_NAME, thread_name, 0, 0, 0);
#endif
>>>>>>> 475d620 (adding code-saleh sabagh)

    while (running) {
        task_t task = task_queue_pop();
        if (task.func) {
            task.func(task.arg);
        }
    }
<<<<<<< HEAD

    free(arg);
=======
>>>>>>> 475d620 (adding code-saleh sabagh)
    return NULL;
}

void co_init() {
<<<<<<< HEAD
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
=======
    if (workers_started) return;
    workers_started = true;

    for (int i = 0; i < WORKER_COUNT; ++i) {
        int *id = malloc(sizeof(int));
        *id = i + 1;
        pthread_create(&workers[i], NULL, worker_thread, id);
>>>>>>> 475d620 (adding code-saleh sabagh)
    }
}

void co(task_func_t func, void *arg) {
    task_queue_push(func, arg);
}

<<<<<<< HEAD

=======
void co_shutdown() {
    running = false;

    pthread_cond_broadcast(&task_queue.cond);

    for (int i = 0; i < WORKER_COUNT; ++i) {
        pthread_join(workers[i], NULL);
    }
}
>>>>>>> 475d620 (adding code-saleh sabagh)
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