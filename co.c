#include "co.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdatomic.h>
#include <signal.h>
#ifdef __linux__
#include <sys/prctl.h>
 //#include <bits/types/sigset_t.h>
// #include <bits/sigaction.h>
#else
// macOS includes
#include <signal.h>
#endif
static pthread_t threads[WORKER_COUNT];
static task_t taskQueue[TASK_QUEUE_SIZE];
static int queueHead = 0;
static int queueTail = 0;
static int queueSize = 0;

static pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t queue_not_empty = PTHREAD_COND_INITIALIZER;
static pthread_cond_t queue_not_full = PTHREAD_COND_INITIALIZER;
static atomic_bool running = true;
static void *dispatcher(void *arg)
{
    while (atomic_load(&running))
    {

        pthread_mutex_lock(&queue_mutex);
        while (queueSize == 0 && atomic_load(&running))
        {
            pthread_cond_wait(&queue_not_empty, &queue_mutex);
        }
        if (queueSize == 0 && !atomic_load(&running))
        {
            pthread_mutex_unlock(&queue_mutex);
            break;
        }
        task_t task = taskQueue[queueHead];
        queueHead = (queueHead + 1) % TASK_QUEUE_SIZE;
        queueSize--;

        pthread_cond_signal(&queue_not_full);
        pthread_mutex_unlock(&queue_mutex);
        if (task.func)
        {
            task.func(task.arg);
        }
    }
}
void co_init()
{
    atomic_store(&running, true);
    for (int i = 0; i < WORKER_COUNT; i++)
    {
        if (pthread_create(&threads[i], NULL, dispatcher, NULL) != 0)
        {
            perror("Failed o create worker thread");
            exit(EXIT_FAILURE);
        }
    }
}

void co_shutdown()
{
    atomic_store(&running, false);
    pthread_mutex_lock(&queue_mutex);
    pthread_cond_broadcast(&queue_not_empty);
    pthread_mutex_unlock(&queue_mutex);
    for (int i = 0; i < WORKER_COUNT; i++)
    {
        pthread_join(threads[i], NULL);
    }
    pthread_mutex_destroy(&queue_mutex);
    pthread_cond_destroy(&queue_not_empty);
    pthread_cond_destroy(&queue_not_full);
}

void co(task_func_t func, void *arg)
{
    task_t task = {
        .func = func,
        .arg = arg};
    pthread_mutex_lock(&queue_mutex);
    while (queueSize == TASK_QUEUE_SIZE && atomic_load(&running))
    {
        pthread_cond_wait(&queue_not_full, &queue_mutex);
    }
    if(!atomic_load(&running)){
        pthread_mutex_unlock(&queue_mutex);
        return;
    }
    taskQueue[queueTail]=task;
    queueTail=(queueTail+1)%TASK_QUEUE_SIZE;
    queueSize++;
    pthread_cond_signal(&queue_not_empty);
    pthread_mutex_unlock(&queue_mutex);

}

int wait_sig()
{
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGTERM);
    pthread_sigmask(SIG_BLOCK, &mask, NULL); // Block signals so they are handled by sigwait
    printf("Waiting for SIGINT (Ctrl+C) or SIGTERM...\n");
    int signum;
    sigwait(&mask, &signum); // Wait for a signal
    printf("Received signal %d, shutting down...\n", signum);
    return signum;
}