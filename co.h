#ifndef CO_H
#define CO_H

#include <pthread.h>
#include <stdbool.h>

#define WORKER_COUNT 32
#define TASK_QUEUE_SIZE 100

typedef void (*task_func_t)(void *arg);

typedef struct {
    task_func_t func;
    void *arg;
} task_t;

void co_init();
void co_shutdown();
void co(task_func_t func, void *arg);

int wait_sig();

#endif // CO_H