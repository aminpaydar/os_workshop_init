#include "co.h"
#include "ch.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdatomic.h>

channel_t *make_ch() {
    // TO BE IMPLEMENTED
    channel_t *ch = malloc(sizeof(channel_t));
    if (ch == NULL) {
        perror("Failed to allocate memory for channel");
        return NULL;
    }
    ch->head = 0;
    ch->tail = 0;
    ch->size = 0;
    if (pthread_mutex_init(&ch->mutex, NULL) != 0) {
        perror("Failed to initialize mutex");
        free(ch);
        return NULL;
    }
    if (pthread_cond_init(&ch->cond_send, NULL) != 0) {
        perror("Failed to initialize send condition variable");
        pthread_mutex_destroy(&ch->mutex);
        free(ch);
        return NULL;
    }
    if (pthread_cond_init(&ch->cond_recv, NULL) != 0) {
        perror("Failed to initialize receive condition variable");
        pthread_cond_destroy(&ch->cond_send);
        pthread_mutex_destroy(&ch->mutex);
        free(ch);
        return NULL;
    }
    for (int i = 0; i < CHANNEL_CAPACITY; i++) {
        ch->buffer[i] = NULL;
    }
    return ch;
    return NULL;
}

void channel_send(channel_t *ch, void *data) {
    if (ch == NULL || data == NULL) {
        if (ch == NULL) {
            fprintf(stderr, "Invalid channel\n");
        }
        if (data == NULL) {
            fprintf(stderr, "Invalid data\n");
        }
        return;
    }
    pthread_mutex_lock(&ch->mutex);
    while (ch->size == CHANNEL_CAPACITY) {
        pthread_cond_wait(&ch->cond_send, &ch->mutex);
    }
    ch->buffer[ch->head] = data;
    ch->head = (ch->head + 1) % CHANNEL_CAPACITY;
    ch->size++;
    pthread_cond_signal(&ch->cond_recv);
    pthread_mutex_unlock(&ch->mutex);
    printf("Sent data: %p\n", data); // Debugging output
    printf("Channel state after send - Head: %d, Tail: %d, Size: %d\n", ch->head, ch->tail, ch->size);
}

void *channel_recv(channel_t *ch) {
    if (ch == NULL) {
        fprintf(stderr, "Invalid channel\n");
        return NULL;
    }
    pthread_mutex_lock(&ch->mutex);
    while (ch->size == 0) {
        pthread_cond_wait(&ch->cond_recv, &ch->mutex);
    }
    void *data = ch->buffer[ch->tail];
    ch->buffer[ch->tail] = NULL; 
    ch->tail = (ch->tail + 1) % CHANNEL_CAPACITY;
    ch->size--;
    pthread_cond_signal(&ch->cond_send);
    pthread_mutex_unlock(&ch->mutex);
    printf("Received data: %p\n", data);
    printf("Channel state after receive - Head: %d, Tail: %d, Size: %d\n", ch->head, ch->tail, ch->size);
   return data;
}
