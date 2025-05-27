#include "co.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdatomic.h>
#include <signal.h>
#ifdef linux
#include <sys/prctl.h>
#else
// macOS includes
#include <signal.h>
#endif


void get_thread_name(char* name) {
#ifdef linux
    prctl(PR_GET_NAME, name, 0, 0, 0);
#else
    // macOS includes
    // strcpy(name, "macOS");
#endif
}

void hello(void *a) {
    int aint = *(int *)a;
    char thread_name[32];
    get_thread_name(thread_name);
    printf("[%s] -> Hello from coroutine %d\n", thread_name, aint);
}

int main() {
    co_init();

    
    for (int i = 1; i <= 100; i++) {
        int *p = malloc(sizeof(int));
        *p = i;
        co(hello, (void *) p);  
    }
    
    int sig = wait_sig();
    co_shutdown();
    return sig;
}