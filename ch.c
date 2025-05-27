#include "ch.h"

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

/* ────────────────────────────────────────────────────────────── */
static void *xmalloc(size_t n)
{
    void *p = malloc(n);
    if (!p) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    return p;
}

/* ────────────────────────────────────────────────────────────── */
channel_t *make_ch(void)
{
    channel_t *ch = (channel_t *)xmalloc(sizeof(channel_t));

    ch->head = ch->tail = ch->size = 0;

    pthread_mutex_init(&ch->mutex, NULL);
    pthread_cond_init(&ch->cond_send, NULL);
    pthread_cond_init(&ch->cond_recv, NULL);

    return ch;
}

/* ────────────────────────────────────────────────────────────── */
void channel_send(channel_t *ch, void *data)
{
    pthread_mutex_lock(&ch->mutex);

    /* Wait while the buffer is full */
    while (ch->size == CHANNEL_CAPACITY)
        pthread_cond_wait(&ch->cond_send, &ch->mutex);

    /* Insert data */
    ch->buffer[ch->tail] = data;
    ch->tail = (ch->tail + 1) % CHANNEL_CAPACITY;
    ch->size++;

    /* Wake one waiting receiver */
    pthread_cond_signal(&ch->cond_recv);
    pthread_mutex_unlock(&ch->mutex);
}

/* ────────────────────────────────────────────────────────────── */
void *channel_recv(channel_t *ch)
{
    pthread_mutex_lock(&ch->mutex);

    /* Wait while the buffer is empty */
    while (ch->size == 0)
        pthread_cond_wait(&ch->cond_recv, &ch->mutex);

    /* Remove data */
    void *data = ch->buffer[ch->head];
    ch->head = (ch->head + 1) % CHANNEL_CAPACITY;
    ch->size--;

    /* Wake one waiting sender (there is now space) */
    pthread_cond_signal(&ch->cond_send);
    pthread_mutex_unlock(&ch->mutex);

    return data;
}