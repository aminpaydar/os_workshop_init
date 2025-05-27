#include "co.h"
#include "ch.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdatomic.h>

//soroosh hadadian 40113006
//zahra ahmadi 40112003

channel_t *make_ch() {
    channel_t *channel = (channel_t *)malloc(sizeof(channel_t));
    if (!channel) {
        perror("Failed to allocate memory for channel");
        return NULL;
    }
    
    channel->head = 0;
    channel->tail = 0;
    channel->size = 0;
    
    pthread_mutex_init(&channel->mutex, NULL);
    pthread_cond_init(&channel->cond_send, NULL);
    pthread_cond_init(&channel->cond_recv, NULL);
    
    return channel;
}

void channel_send(channel_t *channel, void *data) {
    pthread_mutex_lock(&channel->mutex);
    

    while (channel->size == CHANNEL_CAPACITY) {
        pthread_cond_wait(&channel->cond_send, &channel->mutex);
    }
    

    channel->buffer[channel->tail] = data;
    channel->tail = (channel->tail + 1) % CHANNEL_CAPACITY;
    channel->size++;
    
   
    pthread_cond_signal(&channel->cond_recv);
    
    pthread_mutex_unlock(&channel->mutex);
}

void *channel_recv(channel_t *channel) {
    pthread_mutex_lock(&channel->mutex);
    
   
    while (channel->size == 0) {
        pthread_cond_wait(&channel->cond_recv, &channel->mutex);
    }
    
    
    void *data = channel->buffer[channel->head];
    channel->head = (channel->head + 1) % CHANNEL_CAPACITY;
    channel->size--;
    
    
    pthread_cond_signal(&channel->cond_send);
    
    pthread_mutex_unlock(&channel->mutex);
    
    return data;
}