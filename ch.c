#include "ch.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdatomic.h>
//Hirad Harirchi 40113410
//Sarvenaz Haji Mohammad Reza 40112012

channel_t *make_ch() {
    channel_t *ch = (channel_t *)malloc(sizeof(channel_t));
    if (!ch) {
        perror("Failed to allocate memory for channel");
        return NULL;
    }
    
    ch->head = 0;
    ch->tail = 0;
    ch->size = 0;
    
    pthread_mutex_init(&ch->mutex, NULL);
    pthread_cond_init(&ch->cond_send, NULL);
    pthread_cond_init(&ch->cond_recv, NULL);
    
    return ch;
}

void channel_send(channel_t *ch, void *data) {
    pthread_mutex_lock(&ch->mutex);
    
    // Wait until there's space in the buffer
    while (ch->size == CHANNEL_CAPACITY) {
        pthread_cond_wait(&ch->cond_send, &ch->mutex);
    }
    
    // Add data to the buffer
    ch->buffer[ch->tail] = data;
    ch->tail = (ch->tail + 1) % CHANNEL_CAPACITY;
    ch->size++;
    
    // Signal that data is available for receiving
    pthread_cond_signal(&ch->cond_recv);
    
    pthread_mutex_unlock(&ch->mutex);
}

void *channel_recv(channel_t *ch) {
    pthread_mutex_lock(&ch->mutex);
    
    // Wait until there's data to receive
    while (ch->size == 0) {
        pthread_cond_wait(&ch->cond_recv, &ch->mutex);
    }
    
    // Get data from the buffer
    void *data = ch->buffer[ch->head];
    ch->head = (ch->head + 1) % CHANNEL_CAPACITY;
    ch->size--;
    
    // Signal that there's space for sending
    pthread_cond_signal(&ch->cond_send);
    
    pthread_mutex_unlock(&ch->mutex);
    
    return data;
}
