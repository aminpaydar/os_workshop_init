// queue.h
#ifndef QUEUE_H
#define QUEUE_H

#include <stdbool.h>

#define MAX_SIZE 100

// Queue structure definition
typedef struct {
    int items[MAX_SIZE];
    int front;
    int rear;
} Queue;

// Function declarations
void initializeQueue(Queue* q);
bool isEmpty(Queue* q);
bool isFull(Queue* q);
void enqueue(Queue* q, int value);
void dequeue(Queue* q);
int peek(Queue* q);
void printQueue(Queue* q);

#endif // QUEUE_H
