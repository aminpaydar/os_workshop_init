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

static task_queue_t task_queue;
static pthread_t workers[WORKER_COUNT];
static bool running;

bool enqueue_task(task_t task)
{
    pthread_mutex_lock(&task_queue.lock);

    if (task_queue.count < TASK_QUEUE_SIZE)
    {
        task_queue.queue[task_queue.rear] = task;
        task_queue.rear = (task_queue.rear + 1) % TASK_QUEUE_SIZE;
        task_queue.count++;
        pthread_cond_signal(&task_queue.cond);
        pthread_mutex_unlock(&task_queue.lock);
        return true;
    }

    pthread_mutex_unlock(&task_queue.lock);
    return false;
}

bool dequeue_task(task_t *task)
{
    if (task_queue.count == 0)
    {
        return false;
    }

    *task = task_queue.queue[task_queue.front];
    task_queue.front = (task_queue.front + 1) % TASK_QUEUE_SIZE;
    task_queue.count--;
    return true;
}

void *worker_loop(void *arg)
{
    while (1)
    {
        pthread_mutex_lock(&task_queue.lock);

        while (task_queue.count == 0 && running)
        {
            pthread_cond_wait(&task_queue.cond, &task_queue.lock);
        }

        if (!running && task_queue.count == 0)
        {
            pthread_mutex_unlock(&task_queue.lock);
            break;
        }

        task_t task = task_queue.queue[task_queue.front];
        task_queue.front = (task_queue.front + 1) % TASK_QUEUE_SIZE;
        task_queue.count--;

        pthread_mutex_unlock(&task_queue.lock);
        if (task.func)
        {
            task.func(task.arg);
        }
    }

    return NULL;
}

void co_init()
{
    task_queue.front = 0;
    task_queue.rear = 0;
    task_queue.count = 0;
    pthread_mutex_init(&task_queue.lock, NULL);
    pthread_cond_init(&task_queue.cond, NULL);

    running = true;
    for (int i = 0; i < WORKER_COUNT; ++i)
    {
        pthread_create(&workers[i], NULL, worker_loop, NULL);
    }
}

void co_shutdown()
{
    pthread_mutex_lock(&task_queue.lock);
    running = false;
    pthread_cond_broadcast(&task_queue.cond); // Wake up all waiting workers
    pthread_mutex_unlock(&task_queue.lock);

    for (int i = 0; i < WORKER_COUNT; ++i)
    {
        pthread_join(workers[i], NULL);
    }

    pthread_mutex_destroy(&task_queue.lock);
    pthread_cond_destroy(&task_queue.cond);
}

void co(task_func_t func, void *arg)
{
    task_t task;
    task.func = func;
    task.arg = arg;
    if (!enqueue_task(task))
    {
        fprintf(stderr, "Failed to enqueue task\n");
        return;
    }
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