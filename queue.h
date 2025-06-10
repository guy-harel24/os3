#ifndef OS3_QUEUE_H
#define OS3_QUEUE_H
#include "segel.h"
#include <pthread.h>



struct Node {
    int fd;
    struct timeval arrival;
    struct Node* next;
    struct Node* prev;
};

struct Queue {
    struct Node* head;
    struct Node* tail;
    int queue_size;
    int capacity;
};

void queue_init(struct Queue* q,int queue_size) {
    q->head = NULL;
    q->tail = NULL;
    q->queue_size = queue_size;
    q->capacity = 0;
}

int queue_push(struct Queue* q, int fd, struct timeval arrival) {

    if(q->capacity == q->queue_size){
        return 0;
    }

    struct Node* newNode = malloc(sizeof(struct Node));
    newNode->fd = fd;
    newNode->arrival = arrival;
    newNode->next = NULL;
    newNode->prev = q->tail;

    if (q->tail) {
        q->tail->next = newNode;
    } else {
        q->head = newNode;
    }
    q->tail = newNode;

    return 1;
}

void queue_pop(struct Queue* q) {
    if (!q->head) {
        return;
    }
    struct Node* temp = q->head;
    q->head = q->head->next;

    if (q->head) {
        q->head->prev = NULL;
    } else {
        q->tail = NULL;
    }
    free(temp);
}

struct Node* queue_front(struct Queue* q) {
    return q->head;
}


#endif //OS3_QUEUE_H
