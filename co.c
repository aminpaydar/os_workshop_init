#include "co.h"

#include <pthread.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef __linux__
#   include <sys/prctl.h>
#endif

/*──────────── Shared pool state ────────────*/
typedef struct {
    task_t buf[TASK_QUEUE_SIZE];
    size_t head, tail, count;
    pthread_mutex_t mtx;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
} queue_t;

static queue_t q;
static pthread_t workers[WORKER_COUNT];
static atomic_bool shutting_down = ATOMIC_VAR_INIT(false);

/*──────────── Queue helpers ────────────*/
static void enqueue(task_t t) {
    while (q.count == TASK_QUEUE_SIZE && !shutting_down)
        pthread_cond_wait(&q.not_full, &q.mtx);

    if (shutting_down)
        return;

    q.buf[q.tail] = t;
    q.tail = (q.tail + 1) % TASK_QUEUE_SIZE;
    q.count++;
    pthread_cond_signal(&q.not_empty);
}

static bool dequeue(task_t *out) {
    while (q.count == 0 && !shutting_down)
        pthread_cond_wait(&q.not_empty, &q.mtx);

    if (q.count == 0 && shutting_down)
        return false;

    *out = q.buf[q.head];
    q.head = (q.head + 1) % TASK_QUEUE_SIZE;
    q.count--;
    pthread_cond_signal(&q.not_full);
    return true;
}

/*──────────── Worker thread main ────────────*/
static void *worker_main(void *arg) {
    (void)arg;

    static atomic_int id_gen = ATOMIC_VAR_INIT(0);
    char name[16];
    snprintf(name, sizeof(name), "co-%02d", atomic_fetch_add(&id_gen, 1));
    pthread_setname_np(name);

    sigset_t blocked;
    sigemptyset(&blocked);
    sigaddset(&blocked, SIGINT);
    sigaddset(&blocked, SIGTERM);
    pthread_sigmask(SIG_BLOCK, &blocked, NULL);

    while (true) {
        task_t t;
        pthread_mutex_lock(&q.mtx);
        bool ok = dequeue(&t);
        pthread_mutex_unlock(&q.mtx);

        if (!ok) break;
        t.func(t.arg);
    }
    return NULL;
}

/*──────────── Public API ────────────*/
void co_init(void) {
    memset(&q, 0, sizeof(q));
    pthread_mutex_init(&q.mtx, NULL);
    pthread_cond_init(&q.not_empty, NULL);
    pthread_cond_init(&q.not_full, NULL);

    for (int i = 0; i < WORKER_COUNT; ++i) {
        if (pthread_create(&workers[i], NULL, worker_main, NULL) != 0) {
            perror("pthread_create");
            exit(EXIT_FAILURE);
        }
    }
}

void co(task_func_t func, void *arg) {
    if (!func) return;

    pthread_mutex_lock(&q.mtx);
    if (!shutting_down) {
        task_t t = { .func = func, .arg = arg };
        enqueue(t);
    }
    pthread_mutex_unlock(&q.mtx);
}

void co_shutdown(void) {
    atomic_store(&shutting_down, true);

    pthread_mutex_lock(&q.mtx);
    pthread_cond_broadcast(&q.not_empty);
    pthread_cond_broadcast(&q.not_full);
    pthread_mutex_unlock(&q.mtx);

    for (int i = 0; i < WORKER_COUNT; ++i)
        pthread_join(workers[i], NULL);

    pthread_mutex_destroy(&q.mtx);
    pthread_cond_destroy(&q.not_empty);
    pthread_cond_destroy(&q.not_full);
}

int wait_sig(void) {
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGTERM);

    pthread_sigmask(SIG_BLOCK, &mask, NULL);

    puts("Waiting for SIGINT (Ctrl-C) or SIGTERM …");

    int signum;
    if (sigwait(&mask, &signum) == 0) {
        printf("Received signal %d – shutting down …\n", signum);
        return signum;
    }
    perror("sigwait");
    return -1;
}