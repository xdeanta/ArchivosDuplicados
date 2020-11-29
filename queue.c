#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "queue.h"
 
struct queue_node {
    void *item;
    struct queue_node *next;
};

struct _queue {
    struct queue_node *head;
    struct queue_node *tail;
    unsigned int size;

    unsigned int cachedIdx;
    struct queue_node *cachedNode;
};

queue_t *alloc_queue(void) {
    queue_t *q = (queue_t *) malloc(sizeof(struct _queue));

    q->head = NULL;
    q->tail = NULL;
    q->size = 0;

    q->cachedIdx = (unsigned int) -1;
    q->cachedNode = NULL;

    return q;
}
void free_queue(queue_t *q) {
    while(queue_size(q) > 0) {
        queue_pop_front(q);
    }

    free(q);
}

void *queue_pop_front(queue_t *q) {
    struct queue_node *front;
    void *item;

    q->cachedNode = NULL;

    assert(queue_size(q) > 0);

    if(queue_size(q) == 0) {
        return NULL;
    }

    front = q->head;
    q->head = q->head->next;
    q->size--;

    item = front->item;
    free(front);

    if(queue_size(q) == 0) {
        // just cleaning up
        q->head = NULL;
        q->tail = NULL;
    }

    return item;
}


void *queue_at(queue_t *q, unsigned int pos){
    struct queue_node *node;

    if(q == NULL)
    	return NULL;
    
    if(pos >= queue_size(q))
        return NULL;

    if((q->cachedNode != NULL) && ((q->cachedIdx+1) == pos)) {
        node = q->cachedNode->next;
    } else {
        node = q->head;
        while(pos > 0) {
            node = node->next;
            pos--;
        }
    }

    q->cachedNode = node;
    q->cachedIdx = pos;

    return node->item;
}


void queue_push_back(queue_t *q, void *item) {
    struct queue_node *back = (struct queue_node *) malloc(sizeof(struct queue_node));

    q->cachedNode = NULL;

    back->item = item;
    back->next = NULL;
    if(queue_size(q) == 0) {
        q->head = back;
    } else {
        q->tail->next = back;
    }
    q->tail = back;    
    q->size++;
    
}

unsigned int queue_size(queue_t *q) {
    return q->size;
}
