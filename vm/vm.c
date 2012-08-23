#define _POSIX_C_SOURCE 200112L
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "util.h"
#include "vm.h"

#define HOWMANY(s,t) ((s)/sizeof(t))

void queue_init(struct queue *q) {
  q->head = q->tail = 0;
}

struct queue_frame * queue_frame_create() {
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
void queue_stats(struct queue *q) {
  int count = 0;
  
  for(struct queue_frame *r=q->head;r;r=r->next) {
    count += r->tail-r->head;
  }
  printf("queue %p size %d\n",q,count);
}
#endif

void * arena_alloc() {
  void *out;
  if(posix_memalign(&out,ARENA_SIZE,ARENA_SIZE)) {
    die(128,"Out of memory");
  }
  return out;
}

void arena_free(struct arena_header *arena) {
#if DEBUG
  printf("Freeing arena %p (%d)\n",arena,arena->flags);
#endif
  if(arena->list.prev) {
    arena->list.prev->list.next = arena->list.next;
  } else {
    arena->type->members[arena->flags&COHORT_MASK] = arena->list.next;
  }
  if(arena->list.next) {
    arena->list.next->list.prev = arena->list.prev;
  }
  free(arena);
}

void arenas_type_init(struct arenas * arenas,struct arena_type *type,
                      uint32_t code,
                      struct arena_header * (*arena_alloc)(struct arenas *,int),
                      void * (*evacuate)(struct arena_type *,void *),
                      void (*grey_push)(struct arena_type *,void *),
                      void * (*grey_pull)(struct arena_type *),
                      size_t header_size) {
  type->arenas = arenas;
  type->code = code;
  type->from = 0;
  type->header_size = header_size;
  type->to = 0;
  type->free = type->end = 0;
  type->arena_alloc = arena_alloc;
  type->evacuate = evacuate;
  type->grey_push = grey_push;
  type->grey_pull = grey_pull;
  for(int c=0;c<COHORTS;c++) {
    type->members[c] = 0;
  }
}

void arena_init(struct arena_header *header,struct arena_type *type,int fromspace) {
  header->type = type;
  header->flags = fromspace&FROMSPACE_MASK;
  header->list.prev = 0;
  header->list.next = type->members[fromspace&FROMSPACE_MASK];
  if(type->members[fromspace&FROMSPACE_MASK]) {
    type->members[fromspace&FROMSPACE_MASK]->list.prev = header;
  }
  type->members[fromspace&FROMSPACE_MASK] = header;
}

struct arena_header * arena_cons_alloc(struct arenas *arenas,int fromspace) {
  struct arena_cons_header *arena;
  int data_size,usable_data_size;

  arena = arena_alloc();
  arena_init(&(arena->common),&(arenas->cons_type.common),fromspace);
  arena->free = (struct cons *)((void *)arena + sizeof(struct arena_cons_header));
  data_size = ARENA_SIZE - sizeof(struct arena_cons_header);
  usable_data_size = HOWMANY(data_size,struct cons)*sizeof(struct cons);
  arena->end = (struct cons *)((void *)arena->free + usable_data_size);
#if DEBUG
  printf("arena=%p free=%p end=%p\n",arena,arena->free,arena->end);
#endif
  return (struct arena_header *)arena;
}

struct arena_header * arena_ensure_fromspace(struct arena_type *type) {
  if(!type->from) {
    type->from = type->arena_alloc(type->arenas,1);
  }
  return type->from;
}

struct arena_header * arena_ensure_tospace(struct arena_type *type,size_t bytes) {
  void *out;

  // TODO: not ideal to waste the rest of the arena for a large alloc
  if(!type->to || type->free+bytes >= type->end ) {
    type->to = type->arena_alloc(type->arenas,0);
    type->free = (void *)type->to + type->header_size; 
    type->end = (void *)type->to + ARENA_SIZE;
#if DEBUG
    printf("tospace arena=%p free=%p end=%p\n",type->to,type->free,type->end);
#endif
  }
  out = type->free;
  type->free += bytes;
#if STATS
  type->arenas->bytes_copied += bytes;
#endif
  return out;
}

void mark_reference(struct arenas *arenas,void *ref) {
  struct arena_type *type;
  
  type = ARENA_OF(ref)->type;
  
  type->grey_push(type,ref);
}

void * cons_evacuate(struct arena_type *type,void *start) {
  struct cons *from,*to;
  
  from = (struct cons *)start;
  to = (struct cons *)arena_ensure_tospace(type,sizeof(struct cons));
  memcpy(to,from,sizeof(struct cons));
  if(((intptr_t)to->brooks)&1 && to->car.p) {
    mark_reference(type->arenas,&(to->car.p));
  }
  if(((intptr_t)to->brooks)&2 && to->cdr.p) {
    mark_reference(type->arenas,&(to->cdr.p));
  }
  to->brooks = (struct cons *)((intptr_t)to | ((intptr_t)from->brooks&3));
  from->brooks = to;
  return to;
}

void cons_grey_push(struct arena_type *type,void *ref) {
  queue_push(&(((struct arena_cons_type *)type)->grey),ref);
}

void * cons_grey_pull(struct arena_type *type) {
  return queue_pull(&(((struct arena_cons_type *)type)->grey));
}

void * evacuate(void *from) {
  struct arena_header *header;

  header = ARENA_OF(from);
  if(!(header->flags&FROMSPACE_MASK))
    return from;
  return header->type->evacuate(header->type,from);
}

void ** type_grey_pull(struct arena_type *type) {
  return type->grey_pull(type);
}

void * grey_pull(struct arenas *arenas) {
  void *out;
  out = type_grey_pull(&(arenas->cons_type.common));
  if(out) { return out; }
  return 0;
}

int run_evacuations(struct arenas *arenas) {
  void **ptr;

  for(int i=0;i<EVACUATIONS_PER_RUN;i++) {
    ptr = (void **)grey_pull(arenas);
    if(!ptr) {
      return i>0;
    }
    if(ptr) {
      *ptr = evacuate(*ptr);
    }
  }
  return 1;
}

void arenas_cons_init(struct arenas *arenas,struct arena_cons_type *type) {
  arenas_type_init(arenas,&(type->common),ARENA_TYPE_CODE_CONS,
                   arena_cons_alloc,cons_evacuate,
                   cons_grey_push,cons_grey_pull,
                   sizeof(struct arena_cons_header));
  queue_init(&(type->grey));
}

struct cons * cons_alloc(struct arenas *arenas) {
  struct cons * out;
  struct arena_cons_header *h;

  h = (struct arena_cons_header *)arena_ensure_fromspace(&(arenas->cons_type.common));
  out = h->free++;
  if(h->free == h->end) {
    arenas->cons_type.common.from = 0;
  }
  out->brooks = out;
  out->car.p = out->cdr.p = 0;
  return out;
}

struct arenas * arenas_init() {
  struct arenas *arenas;
  arenas = malloc(sizeof(struct arenas));
  arenas->flags = 0;
  arenas_cons_init(arenas,&(arenas->cons_type));
  queue_init(&(arenas->rootset));
  for(int i=0;i<NUM_REGISTERS/BITEL_BITS;i++) {
    arenas->reg_refs[i] = 0;
  }
#if STATS
  arenas->bytes_copied = 0;
#endif
  return arenas;
}

#if STATS
void count(char *start,char *end,int val) {
  char *units = " kMG",*u;
  for(u=units;*(u+1);u++) {
    if(val<2048) {
      break;
    }
    val /= 1024;
  }
  printf("%s%d%c%s",start,val,*u,end);
}

void print_gc_stats(struct arenas * arenas) {
  count("last cycle ","b copied\n",arenas->last_bytes_copied);  
}
#endif

void arenas_type_destroy(struct arena_type *type,int to) {
  for(int c=0;c<COHORTS;c++) {
    if(to || c&FROMSPACE_MASK) { 
      while(type->members[c]) {
        arena_free(type->members[c]);
      }
    }
  }  
}

int pull_from_rootset(struct arenas *arenas) {
  void **member;
  
  member = (void *)queue_pull(&(arenas->rootset));
  if(!member) {
    return 0;
  }
  if(*member) {
    *member = evacuate(*member);
    return 1;
  }
  return 0;
}

void populate_rootset(struct arenas *arenas) {
  /* Registers */
  for(int i=0;i<NUM_REGISTERS;i++) {
    if(arenas->reg_refs[i/BITEL_BITS] & (1<<(i&(BITEL_BITS-1)))) {
      queue_push(&(arenas->rootset),&(arenas->registers[i].r));
    }
  }
}

/* TODO: make this step inceremental */
void free_fromspace(struct arenas *arenas) {
  arenas_type_destroy(&(arenas->cons_type.common),0);
}

void remark_type_to_as_from(struct arena_type *type) {
  for(int g=0;g<GENERATIONS;g++) {
    if(type->members[COHORT(g,0)]) {
      for(struct arena_header *h=type->members[COHORT(g,0)];h;h=h->list.next) {
        h->flags |= FROMSPACE_MASK;
      }
    }
    type->members[COHORT(g,1)] = type->members[COHORT(g,0)];
    type->members[COHORT(g,0)] = 0;
  }
  type->to = 0;
}

void remark_to_as_from(struct arenas *arenas) {
  remark_type_to_as_from(&(arenas->cons_type.common));
}

void start_gc(struct arenas *arenas) {
  populate_rootset(arenas);
  arenas->flags |= FLAG_INGC;
}

void finish_gc(struct arenas *arenas) {
  free_fromspace(arenas);
  remark_to_as_from(arenas);
  arenas->flags &= ~FLAG_INGC;
#if STATS
  arenas->last_bytes_copied = arenas->bytes_copied;
  arenas->bytes_copied = 0;
  print_gc_stats(arenas);
#endif
}

int evacuate_step(struct arenas *arenas) {
  if(!arenas->flags&FLAG_INGC)
    return 0;
  if(run_evacuations(arenas)) { return 1; }
  if(pull_from_rootset(arenas)) { return 1; }
  finish_gc(arenas);
  return 0;
}

int work_for(struct arenas *arenas,int us) {
  uint64_t end;
  int ret = 0;
  
  end = now_usec() + us;
  while((ret |= evacuate_step(arenas)) && now_usec() < end) {
  }
#if DEBUG
  printf("spent %ldus doing gc work (%d)\n",now_usec()-(end-us),ret);
#endif
  return ret;
}

void arenas_cons_destroy(struct arena_cons_type *type) {
  arenas_type_destroy(&(type->common),1);
  queue_destroy(&(type->grey));
}

void arenas_destroy(struct arenas *arenas) {
  queue_destroy(&(arenas->rootset));
  arenas_cons_destroy(&(arenas->cons_type));
  free(arenas);
}

#if STATS
void arenas_type_stats(struct arena_type *type) {
  printf("Type code %d\n",type->code);
  for(int c=0;c<COHORTS;c++) {
    int i = 0;
    for(struct arena_header * h=type->members[c];h;h=h->list.next) {
      i++;
    }
    printf(" cohort %d count %d\n",c,i);
  }  
}

void arenas_stats(struct arenas *arenas) {
  arenas_type_stats(&(arenas->cons_type.common));
}
#endif

void full_gc(struct arenas *arenas) {
#if DEBUG
  printf("Starting requested full, non-incremental GC\n");
#endif
  start_gc(arenas);
  while(work_for(arenas,1000)) {
  }
#if DEBUG
  printf("Finished requested full, non-incremental GC\n");
#endif
}

void main_vm(struct arenas * arenas) {
  full_gc(arenas);
  full_gc(arenas);
}
