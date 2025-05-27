#include "co.h"
#include "ch.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdatomic.h>

channel_t *make_ch() {
    // TO BE IMPLEMENTED
    channel_t ch = {
        .buffer = (void *) malloc(CHANNEL_CAPACITY),
        .cond_recv = PTHREAD_COND_INITIALIZER,
        .cond_send = PTHREAD_COND_INITIALIZER,
        .head = 0, .tail = 0,
        .mutex = PTHREAD_MUTEX_INITIALIZER,
        .size = CHANNEL_CAPACITY
    };
    
    return &ch;
}

void channel_send(channel_t *ch, void *data) {
    
    pthread_mutex_lock(&ch->mutex);
    while (ch->size == CHANNEL_CAPACITY)
        pthread_cond_wait(&ch->cond_send, &ch->mutex);
    
    ch->buffer[ch->tail] = data;
    ch->tail = (ch->tail + 1) % CHANNEL_CAPACITY;
    ch->size ++;
    pthread_cond_signal(&ch->cond_recv);
    pthread_mutex_unlock(&ch->mutex);
}

void *channel_recv(channel_t *ch) {
   
    pthread_mutex_lock(&ch->mutex);
    while (ch->size == 0)
        pthread_cond_wait(&ch->cond_recv, &ch->mutex);
    
    void *data = ch->buffer[ch->head];
    ch->head = (ch->head + 1) % CHANNEL_CAPACITY;
    ch->size --;
    pthread_cond_signal(&ch->cond_send);
    pthread_mutex_unlock(&ch->mutex);

   return data;
}