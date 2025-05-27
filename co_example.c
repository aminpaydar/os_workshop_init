// Ali Ahmadi Esfidi 
// Fereshreh Mokhtarzadeh

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

    int a = 1;
for (int i = 0; i < 100; i++) {
 // A. Allocate memory for this task's argument
int *task_arg = malloc(sizeof(int));
if (!task_arg) {
perror("malloc failed");
continue;
}

// B. Assign the unique value (the loop counter)
*task_arg = i + 1; // Use i+1 for numbers 1 through 100

// C. Pass the pointer to this unique memory to the coroutine
co(hello, (void *)task_arg);
}
    int sig = wait_sig();
     co_shutdown();
     return sig;
}
