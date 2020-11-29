#ifndef __QUEUE_H__
#define __QUEUE_H__

typedef struct _queue queue_t;
 
queue_t *alloc_queue(void);
void free_queue(queue_t *q);

void *queue_pop_front(queue_t *q);
void *queue_at(queue_t *q, unsigned int pos);
void queue_push_back(queue_t *q, void *item);
unsigned int queue_size(queue_t *q);


#endif
