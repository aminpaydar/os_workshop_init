fatemeh abdolhoseini - parastoo afshary

co.c file

#include "co.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <stdatomic.h>
#include <pthread.h>

#ifdef __linux__
#include <sys/prctl.h>
#endif

typedef struct {
    task_t queue[TASK_QUEUE_SIZE];
    int front, back;
    pthread_mutex_t lock;
    pthread_cond_t has_task;
} task_queue_t;

static task_queue_t queue = {
    .front = 0, .back = 0,
    .lock = PTHREAD_MUTEX_INITIALIZER,
    .has_task = PTHREAD_COND_INITIALIZER
};

static pthread_t worker_threads[WORKER_COUNT];
static atomic_bool system_active = true;
static atomic_bool initialized = false;

static void enqueue_task(task_func_t func, void *arg) {
    pthread_mutex_lock(&queue.lock);
    task_t new_task = { .func = func, .arg = arg };
    queue.queue[queue.back] = new_task;
    queue.back = (queue.back + 1) % TASK_QUEUE_SIZE;
    pthread_cond_signal(&queue.has_task);
    pthread_mutex_unlock(&queue.lock);
}

static task_t dequeue_task() {
    pthread_mutex_lock(&queue.lock);
    while (queue.front == queue.back && system_active) {
        pthread_cond_wait(&queue.has_task, &queue.lock);
    }
    task_t t = {0};
    if (queue.front != queue.back) {
        t = queue.queue[queue.front];
        queue.front = (queue.front + 1) % TASK_QUEUE_SIZE;
    }
    pthread_mutex_unlock(&queue.lock);
    return t;
}

static void* worker_loop(void* arg) {
    int id = *(int*)arg;
    free(arg);
#ifdef __linux__
    char name[32];
    snprintf(name, sizeof(name), "Worker-%d", id);
    prctl(PR_SET_NAME, name);
#endif
    while (system_active) {
        task_t t = dequeue_task();
        if (t.func) {
            t.func(t.arg);
        }
    }
    return NULL;
}

void co_init() {
    if (initialized) return;
    for (int i = 0; i < WORKER_COUNT; ++i) {
        int* id = malloc(sizeof(int));
        *id = i + 1;
        pthread_create(&worker_threads[i], NULL, worker_loop, id);
    }
    initialized = true;
}

void co(task_func_t func, void *arg) {
    enqueue_task(func, arg);
}

void co_shutdown() {
    system_active = false;
    pthread_cond_broadcast(&queue.has_task);
    for (int i = 0; i < WORKER_COUNT; ++i) {
        pthread_join(worker_threads[i], NULL);
    }
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

ch.c file 

#include "ch.h"
#include "co.h"
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

channel_t *make_ch() {
    channel_t *ch = (channel_t *)malloc(sizeof(channel_t));
    if (!ch) {
        perror("make_ch malloc");
        return NULL;
    }

    ch->head = 0;
    ch->tail = 0;
    ch->size = 0;

    if (pthread_mutex_init(&ch->mutex, NULL) != 0) {
        perror("make_ch mutex");
        free(ch);
        return NULL;
    }

    if (pthread_cond_init(&ch->cond_send, NULL) != 0) {
        perror("make_ch cond_send");
        pthread_mutex_destroy(&ch->mutex);
        free(ch);
        return NULL;
    }

    if (pthread_cond_init(&ch->cond_recv, NULL) != 0) {
        perror("make_ch cond_recv");
        pthread_cond_destroy(&ch->cond_send);
        pthread_mutex_destroy(&ch->mutex);
        free(ch);
        return NULL;
    }

    return ch;
}

void channel_send(channel_t *ch, void *data) {
    if (!ch) {
        fprintf(stderr, "channel_send: NULL channel\n");
        return;
    }

    pthread_mutex_lock(&ch->mutex);

    while (ch->size == CHANNEL_CAPACITY) {
        pthread_cond_wait(&ch->cond_send, &ch->mutex);
    }

    ch->buffer[ch->tail] = data;
    ch->tail = (ch->tail + 1) % CHANNEL_CAPACITY;
    ch->size++;

    pthread_cond_signal(&ch->cond_recv);
    pthread_mutex_unlock(&ch->mutex);
}

void *channel_recv(channel_t *ch) {
    if (!ch) {
        fprintf(stderr, "channel_recv: NULL channel\n");
        return NULL;
    }

    pthread_mutex_lock(&ch->mutex);

    while (ch->size == 0) {
        pthread_cond_wait(&ch->cond_recv, &ch->mutex);
    }

    void *data = ch->buffer[ch->head];
    ch->head = (ch->head + 1) % CHANNEL_CAPACITY;
    ch->size--;

    pthread_cond_signal(&ch->cond_send);
    pthread_mutex_unlock(&ch->mutex);

    return data;
}
