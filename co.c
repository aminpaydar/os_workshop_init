#define _POSIX_C_SOURCE 200809L

#include "co.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdatomic.h>
#include <pthread.h>
#include <signal.h>

#define MAX_COROUTINES 128

typedef struct {
    coroutine co;
    atomic_int active;
} co_entry;

static co_entry coroutines[MAX_COROUTINES];
static __thread coroutine *current_co = NULL;

int co_create(coroutine *co, coroutine_func fn, void *arg) {
    if (getcontext(&co->ctx) == -1) return -1;
    co->stack = malloc(STACK_SIZE);
    if (!co->stack) return -1;
    co->ctx.uc_stack.ss_sp   = co->stack;
    co->ctx.uc_stack.ss_size = STACK_SIZE;
    co->ctx.uc_link          = &co->caller;
    makecontext(&co->ctx, (void (*)(void))fn, 1, arg);
    return 0;
}

void co_resume(coroutine *co) {
    current_co = co;
    swapcontext(&co->caller, &co->ctx);
}

void co_yield(void) {
    swapcontext(&current_co->ctx, &current_co->caller);
}

void co_init(void) {
    for (int i = 0; i < MAX_COROUTINES; i++) {
        atomic_store(&coroutines[i].active, 0);
        coroutines[i].co.stack = NULL;
    }
}

void co_shutdown(void) {
    for (int i = 0; i < MAX_COROUTINES; i++) {
        if (atomic_load(&coroutines[i].active)) {
            free(coroutines[i].co.stack);
            atomic_store(&coroutines[i].active, 0);
            coroutines[i].co.stack = NULL;
        }
    }
}

void co(task_func_t func, void *arg) {
    for (int i = 0; i < MAX_COROUTINES; i++) {
        if (!atomic_load(&coroutines[i].active)) {
            coroutine *c = &coroutines[i].co;
            if (co_create(c, (coroutine_func)func, arg) == 0) {
                atomic_store(&coroutines[i].active, 1);
                co_resume(c);
            }
            return;
        }
    }
    fprintf(stderr, "No available coroutine slots\n");
}

int wait_sig(void) {
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGTERM);
    sigprocmask(SIG_BLOCK, &mask, NULL);
    printf("Waiting for SIGINT (Ctrl+C) or SIGTERM...\n");
    int signum;
    sigwait(&mask, &signum);
    printf("Received signal %d, shutting down...\n", signum);
    return signum;
}
