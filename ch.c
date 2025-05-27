#include "co.h"
#include "ch.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdatomic.h>

channel_t *make_ch() {
    channel_t *temp_ch;
    temp_ch->head = 0;
    temp_ch->size = 0;
    temp_ch->tail = 0;
    return temp_ch;
}

void channel_send(channel_t *ch, void *data) {
    pthread_mutex_lock(&ch->mutex);
    while(ch->size == CHANNEL_CAPACITY) {
        pthread_cond_wait(&ch->cond_send, &ch->mutex);
    }
    ch->buffer[ch->tail] = data;
    ch->size++;
    ch->tail = (ch->tail + 1)% CHANNEL_CAPACITY;
    pthread_cond_signal(&ch->cond_recv);
    pthread_mutex_unlock(&ch->mutex);
}

void *channel_recv(channel_t *ch) {
   pthread_mutex_lock(&ch->mutex);
    while(ch->size == 0) {
        pthread_cond_wait(&ch->cond_recv, &ch->mutex);
    }
    void * data = ch->buffer[ch->head];
    ch->size--;
    ch->head = (ch->head + 1)% CHANNEL_CAPACITY;
    pthread_cond_signal(&ch->cond_send);
    pthread_mutex_unlock(&ch->mutex);
}
