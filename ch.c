#include "co.h"
#include "ch.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdatomic.h>

channel_t ch ;

channel_t *make_ch() {
    pthread_mutex_init(&ch.mutex, NULL);
    pthread_cond_init(&ch.cond_send, NULL);
    pthread_cond_init(&ch.cond_recv, NULL);
    ch.head = 0;
    ch.tail = CHANNEL_CAPACITY;
    //ch.tail = 0 ; 
    ch.size = 0 ;
    for(int i = 0 ; i < CHANNEL_CAPACITY ; i++){
        //*(ch.buffer + i) = NULL ;
        ch.buffer[i] = NULL;
    }

    return &ch;
}

// برای کانسیومر باید یه ویت باشه که وقتی چیزی 
// اومد این متوجه بشه و یکی از اون پرودیوسر ها گذاشت یه سیگنال بده
void channel_send(channel_t *ch, void *data) {
    pthread_mutex_lock(&ch->mutex);
    while(ch->size == CHANNEL_CAPACITY){
        pthread_cond_wait(&ch->cond_recv, &ch->mutex);
    }
    //printf("%d",ch->tail);
    ch->buffer[ch->tail] = data;
    ch->tail = (ch->tail + 1) % CHANNEL_CAPACITY;
    ch->size++;
    pthread_cond_signal(&ch->cond_send);
    pthread_mutex_unlock(&ch->mutex);
}

void *channel_recv(channel_t *ch) {
    pthread_mutex_lock(&ch->mutex);
    while(ch->size == 0){
        pthread_cond_wait(&ch->cond_send, &ch->mutex);
    }
    /*
    
    while(ch->head == ch->tail){
        pthread_cond_wait(&ch.cond_recv , &ch.mutex );
    }

    */
    printf("%d",ch->head);
    void *data = ch->buffer[ch->head];
    ch->head = (ch->head + 1) % CHANNEL_CAPACITY;
    ch->size--;
    pthread_cond_signal(&ch->cond_recv);
    pthread_mutex_unlock(&ch->mutex);
    return data;
}