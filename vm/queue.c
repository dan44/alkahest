#include <stdio.h>
#include <stdlib.h>

#include "vm.h"

struct queue_frame {
  struct queue_frame *next;
  void **head,**tail,**end;
};

void queue_init(struct queue *q) {
  q->head = q->tail = 0;
}

static struct queue_frame * queue_frame_create() {
  struct queue_frame *frame;

  if(!(frame = malloc(QUEUE_FRAME_SIZE))) {
    die(128,"Out of memory");
  }
  frame->end = (void *)frame + QUEUE_FRAME_SIZE;
  frame->head = frame->tail = (void*)(frame + 1);
  frame->next = 0;
#if DEBUG
  printf("Queue frame frame=%p head/tail = %p end = %p\n",frame,frame->head,frame->end);
#endif
  return frame;
}

void queue_destroy(struct queue *q) {
  struct queue_frame *old;
  
  while(q->head) {
    old = q->head;
    q->head = q->head->next;
    free(old);
  }
}

void queue_push(struct queue *q,void *entry) {
#if DEBUG
  if(!entry) {
    die(255,"Refusing to put NULL on queue");
  }
#endif
  if(!q->tail) {
    q->head = q->tail = queue_frame_create();
  }
  if(q->tail->tail >= q->tail->end) {
    q->tail->next = queue_frame_create();
    q->tail = q->tail->next;
  }
  *(q->tail->tail++) = entry;
}

static void queue_skip_empty(struct queue *q) {
  struct queue_frame *empty;
  
  while(q->head && q->head->head == q->head->tail) {
    empty = q->head;
    q->head = q->head->next;
    free(empty);
  }
  if(!q->head) {
    q->tail = 0;
  }
}

void * queue_pull(struct queue *q) {
  void *out;

  if(!q->head) {
    return 0;
  }
  out = *(q->head->head++);
  queue_skip_empty(q);
  return out;
}

#if DEBUG
int queue_size(struct queue *q) {
  int count = 0;
  
  for(struct queue_frame *r=q->head;r;r=r->next) {
    count += r->tail-r->head;
  }
  return count;
}
#endif
