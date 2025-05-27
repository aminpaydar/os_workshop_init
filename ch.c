#include "co.h"
#include "ch.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdatomic.h>

channel_t *make_ch();
void channel_send(channel_t *ch, void *data);
void *channel_recv(channel_t *ch);
int IsFull(channel_t *ch );
int IsEmpty(channel_t *ch);

channel_t *make_ch() {
    channel_t *output = malloc(sizeof(channel_t));
    if(!output) return NULL;
    output->head = 0;
    output->tail = 0;
    output->size = 0;
    pthread_mutex_init(&output->mutex , NULL);
    pthread_cond_init(&output->cond_send , NULL);
    pthread_cond_init(&output->cond_recv , NULL);
    return output;
}

void channel_send(channel_t *ch, void *data) {
    pthread_mutex_lock(&ch->mutex);
    while(IsFull(ch)) pthread_cond_wait(&ch->cond_send , &ch->mutex);

    ch->buffer[ch->tail] = data;
    ch->tail++;
    ch->tail %= CHANNEL_CAPACITY;
    ch->size++;
    pthread_cond_signal(&ch->cond_recv);
    pthread_mutex_unlock(&ch->mutex);
}

void *channel_recv(channel_t *ch) {
    pthread_mutex_lock(&ch->mutex);
    while(IsEmpty(ch)) pthread_cond_wait(&ch->cond_recv , &ch->mutex);

    void* output = ch->buffer[ch->head];
    ch->head++;
    ch->head %= CHANNEL_CAPACITY;
    ch->size--;
    pthread_cond_signal(&ch->cond_send);
    pthread_mutex_unlock(&ch->mutex);
    return output;

}


int IsFull(channel_t *ch ) {
    return ch->size == CHANNEL_CAPACITY;
}
int IsEmpty(channel_t *ch) {
    return ch->size == 0;
}