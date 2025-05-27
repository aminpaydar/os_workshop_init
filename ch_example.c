#include "ch.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>

channel_t *ch;

void *producer(void *arg) {
    for (int i = 0; i < 5; i++) {
        printf("Producer: Sending %d\n", i);
        channel_send(ch, (void *)(long)i);
        sleep(1);
    }
    return NULL;
}

void *consumer(void *arg) {
    for (int i = 0; i < 5; i++) {
        int data = (int)(long)channel_recv(ch);
        printf("Consumer: Received %d\n", data);
    }
    return NULL;
}

int main() {
    ch = make_ch();
    if (!ch) {
        fprintf(stderr, "Failed to create channel\n");
        return 1;
    }

    pthread_t prod_thread, cons_thread;
    if (pthread_create(&prod_thread, NULL, producer, NULL) != 0) {
        fprintf(stderr, "Failed to create producer thread\n");
        destroy_ch(ch);
        return 1;
    }
    if (pthread_create(&cons_thread, NULL, consumer, NULL) != 0) {
        fprintf(stderr, "Failed to create consumer thread\n");
        pthread_cancel(prod_thread);
        destroy_ch(ch);
        return 1;
    }

    pthread_join(prod_thread, NULL);
    pthread_join(cons_thread, NULL);

    destroy_ch(ch);
    return 0;
}