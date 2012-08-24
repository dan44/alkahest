#ifndef QUEUE_H
#define QUEUE_H

struct queue {
  struct queue_frame *head,*tail;
};

void queue_init(struct queue *);
void queue_destroy(struct queue *);
void queue_push(struct queue *,void *);
void * queue_pull(struct queue *);
int queue_size(struct queue *);

#endif
