#include "queue.h"

#include <pthread.h>
#include <stdlib.h>

typedef struct node {
    void *value;
    struct node *nextptr;
} node_t;

typedef struct queue {
    node_t *queue_head;
    node_t *queue_tail;
    pthread_mutex_t lock;
    pthread_cond_t cond;
} queue_t;

queue_t *queue_init(void) {
    queue_t *queue_ptr = (queue_t *) calloc(1, sizeof(queue_t));
    if (queue_ptr == NULL) {
        return NULL;
    }
    queue_ptr->queue_head = NULL;
    queue_ptr->queue_tail = NULL;
    pthread_mutex_init(&queue_ptr->lock, NULL);
    pthread_cond_init(&queue_ptr->cond, NULL);
    return queue_ptr;
}

void queue_enqueue(queue_t *queue, void *value) {
    pthread_mutex_lock(&queue->lock);
    node_t *node = malloc(sizeof(node_t));
    node->value = value;
    node->nextptr = NULL;
    if ((queue->queue_head == NULL) && (queue->queue_tail == NULL)) {
        queue->queue_head = node;
    }
    else {
        queue->queue_tail->nextptr = node;
    }
    queue->queue_tail = node;
    pthread_cond_signal(&queue->cond);
    pthread_mutex_unlock(&queue->lock);
}

void *queue_dequeue(queue_t *queue) {
    if (queue == NULL) {
        return NULL;
    }
    pthread_mutex_lock(&queue->lock);
    while ((queue->queue_head == NULL) && (queue->queue_tail == NULL)) {
        pthread_cond_wait(&queue->cond, &queue->lock);
    }
    node_t *first_head = queue->queue_head;
    void *value = first_head->value;

    if (queue->queue_head == queue->queue_tail) {
        free(queue->queue_head);
        queue->queue_head = NULL;
        queue->queue_tail = NULL;
        pthread_mutex_unlock(&queue->lock);
        return value;
    }
    queue->queue_head = queue->queue_head->nextptr;
    free(first_head);
    pthread_mutex_unlock(&queue->lock);
    return value;
}

void queue_free(queue_t *queue) {
    free(queue);
}