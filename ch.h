#ifndef CH_H
#define CH_H

#include <pthread.h>
#include <stdbool.h>

#define CHANNEL_CAPACITY 10

typedef struct {
    void *buffer[CHANNEL_CAPACITY];
    int head, tail, size;
    pthread_mutex_t mutex;
    pthread_cond_t cond_send, cond_recv;
} channel_t;

channel_t *make_ch();
void channel_send(channel_t *ch, void *data);
void *channel_recv(channel_t *ch);

#endif // CH_H