#include "co.h"
#include "ch.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdatomic.h>

channel_t *make_ch() {
    // TO BE IMPLEMENTED
    channel_t *ch = malloc(sizeof(channel_t));
    ch->head = 0;
    ch->tail = 0;
    ch->size = 0;
    pthread_mutex_init(&ch->mutex, NULL);
    pthread_cond_init(&ch->cond_send, NULL);
    pthread_cond_init(&ch->cond_recv, NULL);
    return ch;
}

void channel_send(channel_t *ch, void *data) {
    // TO BE IMPLEMENTED
    pthread_mutex_lock(&ch->mutex);
        while (ch->size == 10){
            pthread_cond_wait(&ch->cond_send, &ch->mutex);
        }
        ch->buffer[ch->tail] = data;
        ch->tail = (ch->tail +1) % CHANNEL_CAPACITY;
        ch->size ++;
        pthread_cond_signal(&ch->cond_recv);
    pthread_mutex_unlock(&ch->mutex);

}

void *channel_recv(channel_t *ch) {
   // TO BE IMPLEMENTED

   pthread_mutex_lock(&ch->mutex);
   while (ch->size == 0){
    pthread_cond_wait(&ch->cond_recv, &ch->mutex);

   }

   ch->head = (ch->head + 1) % CHANNEL_CAPACITY;
   ch->size --;
    pthread_cond_signal(&ch->cond_send);

   pthread_mutex_unlock(&ch->mutex);
   return ch->buffer[ch->head];
}
