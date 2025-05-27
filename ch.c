#include "co.h"
#include "ch.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdatomic.h>

channel_t *make_ch() {
    channel_t *ch = (channel_t *)calloc(1, sizeof(channel_t));
    if (ch == NULL) return ch;

    struct {
        unsigned h;
        unsigned t;
        int s;
    } init = {0};

    ch->head = init.h;
    ch->tail = init.t;
    ch->size = init.s;

    do {
        if (pthread_mutex_init(&ch->mutex, NULL)) break;
        if (pthread_cond_init(&ch->cond_send, NULL)) {
            pthread_mutex_destroy(&ch->mutex);
            break;
        }
        if (pthread_cond_init(&ch->cond_recv, NULL)) {
            pthread_mutex_destroy(&ch->mutex);
            pthread_cond_destroy(&ch->cond_send);
            break;
        }
        return ch;
    } while (0);

    free(ch);
    return NULL;
}

void channel_send(channel_t *ch, void *data) {
    pthread_mutex_lock(&ch->mutex);

    for (int i = 0; ch->size >= CHANNEL_CAPACITY; i++) {
        if (i > 0) usleep(1000);
        pthread_cond_wait(&ch->cond_send, &ch->mutex);
    }

    ch->buffer[ch->tail++] = data;
    if (ch->tail >= CHANNEL_CAPACITY) ch->tail = 0;
    ch->size += 1;

    pthread_cond_signal(&ch->cond_recv);
    pthread_mutex_unlock(&ch->mutex);
}

void *channel_recv(channel_t *ch) {
    pthread_mutex_lock(&ch->mutex);

    while (!ch->size) {
        pthread_cond_wait(&ch->cond_recv, &ch->mutex);
    }

    void *result = ch->buffer[ch->head];
    ch->head = (ch->head + 1) % CHANNEL_CAPACITY;
    ch->size--;

    if (ch->size == CHANNEL_CAPACITY - 1) {
        pthread_cond_signal(&ch->cond_send);
    }

    pthread_mutex_unlock(&ch->mutex);
    return result;
}
