#define _POSIX_C_SOURCE 200112L
#define DEBUG 1
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

void * queue_pull(struct queue *q) {
  struct queue_frame *empty;

  while(q->head && q->head->head == q->head->tail) {
    empty = q->head;
    q->head = q->head->next;
    free(empty);
  }
  if(!q->head) {
    q->tail = 0;
    return 0;
  }
  return *(q->head->head++);
}

void * arena_alloc() {
  void *out;
  if(posix_memalign(&out,ARENA_SIZE,ARENA_SIZE)) {
    die(128,"Out of memory");
  }
  return out;
}

void arena_free(struct arena_header *arena) {
#if DEBUG
  printf("Freeing arena %p\n",arena);
#endif
  if(arena->list.prev) {
    arena->list.prev->list.next = arena->list.next;
  } else {
    arena->type->members[arena->gen][arena->tospace] = arena->list.next;
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
                      size_t header_size) {
  type->arenas = arenas;
  type->code = code;
  type->from = 0;
  type->header_size = header_size;
  type->to = 0;
  type->free = type->end = 0;
  type->arena_alloc = arena_alloc;
  type->evacuate = evacuate;
  for(int g=0;g<GENERATIONS;g++) {
    for(int t=0;t<2;t++) {
      type->members[g][t] = 0;
    }
  }
}

void arena_init(struct arena_header *header,struct arena_type *type,int tospace) {
  header->type = type;
  header->gen = 0;
  header->tospace = tospace;
  header->list.prev = 0;
  header->list.next = type->members[0][tospace];
  if(type->members[0][tospace]) {
    type->members[0][tospace]->list.prev = header;
  }
  type->members[0][tospace] = header;
}

struct arena_header * arena_cons_alloc(struct arenas *arenas,int tospace) {
  struct arena_cons_header *arena;
  int data_size,usable_data_size;

  arena = arena_alloc();
  arena_init(&(arena->common),&(arenas->cons_type.common),tospace);
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
    type->from = type->arena_alloc(type->arenas,0);
  }
  return type->from;
}

struct arena_header * arena_ensure_tospace(struct arena_type *type,size_t bytes) {
  void *out;

  // TODO: not ideal to waste the rest of the arena for a large alloc
  if(!type->to || type->free+bytes >= type->end ) {
    type->to = type->arena_alloc(type->arenas,1);
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
  queue_push(&(arenas->grey),ref);
}

void * cons_evacuate(struct arena_type *type,void *start) {
  struct cons *from,*to;
  
  from = (struct cons *)start;
  to = (struct cons *)arena_ensure_tospace(type,sizeof(struct cons));
  memcpy(to,from,sizeof(struct cons));
  from->brooks = to;
  if(((intptr_t)to->brooks)&1 && to->car.p) {
    mark_reference(type->arenas,&(to->car.p));
  }
  if(((intptr_t)to->brooks)&2 && to->cdr.p) {
    mark_reference(type->arenas,&(to->cdr.p));
  }
  from->brooks = to;
  return to;
}

void * evacuate(void *from) {
  struct arena_header *header;
  
  header = (struct arena_header *)((intptr_t)from&(~(ARENA_SIZE-1)));
  if(header->tospace)
    return from;
  return header->type->evacuate(header->type,from);
}

int run_evacuations(struct arenas *arenas) {
  void **ptr;

  for(int i=0;i<EVACUATIONS_PER_RUN;i++) {
    ptr = (void **)queue_pull(&(arenas->grey));
    if(!ptr) {
      printf("No more evacuations\n");
      return 0;
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
                   sizeof(struct arena_cons_header));
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
  arenas_cons_init(arenas,&(arenas->cons_type));
  queue_init(&(arenas->grey));
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
  for(int g=0;g<GENERATIONS;g++) {
    for(int t=0;t<to+1;t++) {
      while(type->members[g][t]) {
        arena_free(type->members[g][t]);
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
  *member = evacuate(*member);
  return 1;
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
    if(type->members[g][1]) {
      for(struct arena_header *h=type->members[g][1];h;h=h->list.next) {
        h->tospace=0;
      }
    }
    type->members[g][0] = type->members[g][1];
    type->members[g][1] = 0;
  }
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
  if(arenas->flags&FLAG_INGC) {
    finish_gc(arenas);
    return 0;
  }
  return 0;
}

int work_for(struct arenas *arenas,int us) {
  uint64_t end;
  int ret = 0;
  
  end = now_usec() + us;
  while(evacuate_step(arenas) && now_usec() < end) {
    ret = 1;
  }
#if DEBUG
  printf("spent %ldus doing gc work\n",now_usec()-(end-us));
#endif
  return ret;
}

void arenas_destroy(struct arenas *arenas) {
  queue_destroy(&(arenas->grey));
  queue_destroy(&(arenas->rootset));
  arenas_type_destroy(&(arenas->cons_type.common),1);
  free(arenas);
}

void main_vm(struct arenas * arenas) {
  start_gc(arenas);
  while(work_for(arenas,1000)) {
  }
  printf("Mark\n");
  start_gc(arenas);
  while(work_for(arenas,1000)) {
  }
}
