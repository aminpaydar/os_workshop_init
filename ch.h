#ifndef CH_H
#define CH_H
/*
 * Very small blocking channel (bounded queue)
 *   • Fixed capacity (CHANNEL_CAPACITY)
 *   • channel_send() blocks when full
 *   • channel_recv() blocks when empty
 */

#include <pthread.h>

#define CHANNEL_CAPACITY 10

typedef struct {
    void *buffer[CHANNEL_CAPACITY];   /* circular buffer               */
    int   head, tail, size;           /* queue indices / count         */
    pthread_mutex_t mutex;            /* protects all queue state      */
    pthread_cond_t  cond_send;        /* signalled when space appears  */
    pthread_cond_t  cond_recv;        /* signalled when data appears   */
} channel_t;

/* Construct a new channel (heap-allocated). */
channel_t *make_ch(void);

/* Blocking send / receive */
void  channel_send(channel_t *ch, void *data);
void *channel_recv(channel_t *ch);

#endif /* CH_H */