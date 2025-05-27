#include "co.h"
#include "ch.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdatomic.h>

channel_t *ch;

void producer() {
    for (int i = 0; i < 5; i++) {
        printf("Producer: Sending %d\n", i);
        channel_send(ch, (void *)(long)i);
        sleep(1);
    }
}

void consumer() {
    for (int i = 0; i < 5; i++) {
        int data = (int)(long)channel_recv(ch);
        printf("Consumer: Received %d\n", data);
    }
}

int main() {
    co_init();
    printf("co initialized\n");
    
    ch = make_ch();
    printf("Channel created\n");

    co(producer, NULL);
    co(consumer, NULL);
    
    int sig = wait_sig();

    co_shutdown();
    printf("co shut down\n");

    del_ch(ch);
    printf("Channel deleted\n");
    
    return sig;
}
