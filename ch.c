#include "ch.h"       
#include "co.h"       
#include <stdlib.h>   
#include <stdio.h>    
#include <pthread.h>  


channel_t *make_ch() {
    channel_t *ch = (channel_t *)malloc(sizeof(channel_t));
    if (ch == NULL) {
        return NULL;
    }

    ch->head = 0;
    ch->tail = 0;
    ch->size = 0;

    if (pthread_mutex_init(&ch->mutex, NULL) != 0) {
        free(ch);
        return NULL;
    }

    if (pthread_cond_init(&ch->cond_send, NULL) != 0) {
        pthread_mutex_destroy(&ch->mutex); 
        free(ch);
        return NULL;
    }

    if (pthread_cond_init(&ch->cond_recv, NULL) != 0) {
        pthread_cond_destroy(&ch->cond_send);  
        pthread_mutex_destroy(&ch->mutex);
        free(ch);
        return NULL;
    }

    return ch;
}

void channel_send(channel_t *ch, void *data) {
    if (ch == NULL) {
        fprintf(stderr, "Error: channel_send called on NULL channel.\n");
        return;
    }

    if (pthread_mutex_lock(&ch->mutex) != 0) {
        perror("channel_send: Failed to lock mutex");
        return;
    }

    while (ch->size == CHANNEL_CAPACITY) {
        if (pthread_cond_wait(&ch->cond_send, &ch->mutex) != 0) {
            perror("channel_send: Failed to wait on cond_send");
            pthread_mutex_unlock(&ch->mutex); 
            return;
        }
    }

    ch->buffer[ch->tail] = data;
    ch->tail = (ch->tail + 1) % CHANNEL_CAPACITY;
    ch->size++;

}

void *channel_recv(channel_t *ch) {
    if (ch == NULL) {
        fprintf(stderr, "Error: channel_recv called on NULL channel.\n");
        return NULL;
    }

    void *data = NULL;

    if (pthread_mutex_lock(&ch->mutex) != 0) {
        perror("channel_recv: Failed to lock mutex");
        return NULL; 
    }

    while (ch->size == 0) {
        if (pthread_cond_wait(&ch->cond_recv, &ch->mutex) != 0) {
            perror("channel_recv: Failed to wait on cond_recv");
            pthread_mutex_unlock(&ch->mutex); 
            return NULL;
        }
    }

    data = ch->buffer[ch->head];
    ch->head = (ch->head + 1) % CHANNEL_CAPACITY;
    ch->size--;

    if (pthread_cond_signal(&ch->cond_send) != 0) {
        perror("channel_recv: Failed to signal cond_send");
    }

    if (pthread_mutex_unlock(&ch->mutex) != 0) {
        perror("channel_recv: Failed to unlock mutex");
    }

    return data;
}

