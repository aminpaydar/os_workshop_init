#ifndef CO_H
#define CO_H

#include <ucontext.h>

#define STACK_SIZE (64*1024)

typedef void (*task_func_t)(void *arg);
typedef void (*coroutine_func)(void *arg);

typedef struct {
    ucontext_t ctx;
    ucontext_t caller;
    void *stack;
} coroutine;

void co_init(void);
void co_shutdown(void);
void co(task_func_t func, void *arg);
int co_create(coroutine *co, coroutine_func fn, void *arg);
void co_resume(coroutine *co);
void co_yield(void);
int wait_sig(void);

#endif
