#ifndef CO_H
#define CO_H
/*
 * Simple coroutine-style thread pool
 * ─────────────────────────────────
 *  • Call  co_init()     once at program start.
 *  • Submit work with    co(task_func_t, void*).
 *  • Call  co_shutdown() before exit (or after wait_sig()).
 *
 * Author:  (you)
 */

#include <pthread.h>

#define WORKER_COUNT     32     /* number of worker threads            */
#define TASK_QUEUE_SIZE 100     /* bounded queue capacity              */

typedef void (*task_func_t)(void *arg);

typedef struct {
    task_func_t func;
    void       *arg;
} task_t;

/* pool control */
void co_init(void);
void co_shutdown(void);
void co(task_func_t func, void *arg);

/* utility: block until SIGINT/SIGTERM, return the signal number */
int wait_sig(void);

#endif /* CO_H */