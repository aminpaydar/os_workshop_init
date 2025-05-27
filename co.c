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

struct Queue {
     task_t arr[TASK_QUEUE_SIZE];
     int back;
     int front;
};
struct Queue task_queue;
pthread_t threads[WORKER_COUNT];

void* worker_thread(void *arg) {
    // TO BE IMPLEMENTED: Worker thread function that processes tasks from the queue
    // This function should run in a loop, waiting for tasks to be added to the queue
    // and executing them as they come in.
    while (true) {
        // Wait for a task to be available in the queue
        if (task_queue.back != task_queue.front) {
            task_t task;
            task = task_queue.arr[task_queue.front];
            task_queue.front = (task_queue.front + 1) % TASK_QUEUE_SIZE; // Move front pointer
            // Execute the task function with its argument
            if (task.func) {
                task.func(task.arg);
            }
        } else
        {
            continue; // If no task is available, continue waiting
        }
        
        
        // Execute the task
        // If a shutdown signal is received, break the loop and exit
        // TO BE IMPLEMENTED: Check for shutdown signal and exit if received
    }
    return NULL;
}



void co_init() {

    task_queue.back = 0;
    task_queue.front = 0;
    int idx = 0;
    for (int i = 0; i < WORKER_COUNT; i++) {
        pthread_create(&threads[i], NULL, worker_thread, NULL);
    }
    // Initialize the queue and worker threads
    // TO BE IMPLEMENTED thread pool create bcondition_variable_waite andaze worker count thread ye saf be andaze worker count functione thread masalan tread_worker ke toosh ye while ke as sare safe ye task var dare va funcctionesho call cone

}

void co_shutdown() {



    for (int i = 0; i < WORKER_COUNT; i++) {
        pthread_cancel(threads[i]); // Cancel each worker thread
    }
    for (int i = 0; i < WORKER_COUNT; i++) {
        pthread_join(threads[i], NULL); // Wait for each worker thread to finish
    }
    task_queue.back = 0; // Reset the queue
    task_queue.front = 0; // Reset the queue
    printf("Worker threads shut down and resources cleaned up.\n");
}

void co(task_func_t func, void *arg) {
    task_queue.arr[task_queue.back].func = func; // Add the function to the queue
    task_queue.arr[task_queue.back].arg = arg; // Add the argument to the queue
    task_queue.back = (task_queue.back + 1) % TASK_QUEUE_SIZE; // Move back pointer
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