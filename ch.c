#include "ch.h"       
#include "co.h"       
#include <stdlib.h>   // For malloc, free
#include <stdio.h>    // For perror (error reporting)
#include <pthread.h>  // For mutexes and condition variables


channel_t *make_ch() {
    channel_t *ch = (channel_t *)malloc(sizeof(channel_t));
    if (ch == NULL) {
        perror("Failed to allocate memory for channel");
        return NULL;
    }

    ch->head = 0;
    ch->tail = 0;
    ch->size = 0;

    if (pthread_mutex_init(&ch->mutex, NULL) != 0) {
        perror("Failed to initialize channel mutex");
        free(ch);
        return NULL;
    }

    // Condition variable for senders (wait if channel is full)
    if (pthread_cond_init(&ch->cond_send, NULL) != 0) {
        perror("Failed to initialize channel send condition variable");
        pthread_mutex_destroy(&ch->mutex); // Clean up previously initialized mutex
        free(ch);
        return NULL;
    }

    // Condition variable for receivers (wait if channel is empty)
    if (pthread_cond_init(&ch->cond_recv, NULL) != 0) {
        perror("Failed to initialize channel receive condition variable");
        pthread_cond_destroy(&ch->cond_send);  // Clean up previously initialized cond_send
        pthread_mutex_destroy(&ch->mutex); // Clean up previously initialized mutex
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
        // Potentially a more robust error handling strategy is needed here,
        // as the channel state might be inconsistent.
        return;
    }

    // Wait while the channel buffer is full
    // The while loop is important to handle spurious wakeups from pthread_cond_wait
    while (ch->size == CHANNEL_CAPACITY) {
        // printf("Channel full. Sender waiting...\n"); // For debugging
        if (pthread_cond_wait(&ch->cond_send, &ch->mutex) != 0) {
            perror("channel_send: Failed to wait on cond_send");
            pthread_mutex_unlock(&ch->mutex); // Unlock before returning on error
            return;
        }
    }

    // Add data to the buffer (at the tail)
    ch->buffer[ch->tail] = data;
    ch->tail = (ch->tail + 1) % CHANNEL_CAPACITY;
    ch->size++;

    // Signal a waiting receiver that data is available
    if (pthread_cond_signal(&ch->cond_recv) != 0) {
        perror("channel_send: Failed to signal cond_recv");
    }

    if (pthread_mutex_unlock(&ch->mutex) != 0) {
        perror("channel_send: Failed to unlock mutex");
    }
}

void *channel_recv(channel_t *ch) {
    if (ch == NULL) {
        fprintf(stderr, "Error: channel_recv called on NULL channel.\n");
        return NULL;
    }

    void *data = NULL;

    if (pthread_mutex_lock(&ch->mutex) != 0) {
        perror("channel_recv: Failed to lock mutex");
        return NULL; // Error acquiring lock
    }

    // Wait while the channel buffer is empty
    // The while loop handles spurious wakeups
    while (ch->size == 0) {
        // printf("Channel empty. Receiver waiting...\n"); // For debugging
        if (pthread_cond_wait(&ch->cond_recv, &ch->mutex) != 0) {
            perror("channel_recv: Failed to wait on cond_recv");
            pthread_mutex_unlock(&ch->mutex); // Unlock before returning on error
            return NULL;
        }
        // After waking up, re-check the condition (ch->size == 0).
        // If another thread also woke up and took the item, or if it was a spurious wakeup,
        // this loop ensures we only proceed if data is actually available.
    }

    // Retrieve data from the buffer (from the head)
    data = ch->buffer[ch->head];
    ch->head = (ch->head + 1) % CHANNEL_CAPACITY;
    ch->size--;

    // Signal a waiting sender that space is available
    if (pthread_cond_signal(&ch->cond_send) != 0) {
        perror("channel_recv: Failed to signal cond_send");
    }

    if (pthread_mutex_unlock(&ch->mutex) != 0) {
        perror("channel_recv: Failed to unlock mutex");
    }

    return data;
}

