#define _POSIX_C_SOURCE 200112L
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "util.h"
#include "vm.h"
#include "queue.h"

#define HOWMANY(s,t) ((s)/sizeof(t))

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
                      void (*scavange)(struct arena_type *,void *),
                      void (*grey_push)(struct arena_type *,void *),
                      void * (*grey_pull)(struct arena_type *),
                      size_t header_size) {
  type->arenas = arenas;
  type->code = code;
  type->header_size = header_size;
  for(int c=0;c<2;c++) {  
    type->current[c].arena = 0;
    type->current[c].free = type->current[c].end = 0;
  }
  type->arena_alloc = arena_alloc;
  type->evacuate = evacuate;
  type->scavange = scavange;
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
  int data_size,usable_data_size;
  struct arena_current *current;

  current = &(arenas->cons_type.common.current[fromspace]);
  current->arena = arena_alloc();
  arena_init(current->arena,&(arenas->cons_type.common),fromspace);
  current->free = (struct cons *)((void *)current->arena + sizeof(struct arena_cons_header));
  data_size = ARENA_SIZE - sizeof(struct arena_cons_header);
  usable_data_size = HOWMANY(data_size,struct cons)*sizeof(struct cons);
  current->end = (struct cons *)((void *)current->free + usable_data_size);
#if DEBUG
  printf("arena=%p free=%p end=%p\n",current->arena,current->free,current->end);
#endif
  return (struct arena_header *)current->arena;
}

void * arena_ensure_space(struct arena_type *type,size_t bytes,int from,int copy) {
  void *out;
  struct arena_current *current;

  from=!!from;
  // TODO: not ideal to waste the rest of the arena for a large alloc
  current = &(type->current[from]);
  if(!current->arena || current->free + bytes >= current->end ) {
    current->arena = type->arena_alloc(type->arenas,from);
    current->free = (void *)current->arena + type->header_size; 
    current->end = (void *)current->arena + ARENA_SIZE;
#if DEBUG
    printf("tospace arena=%p free=%p end=%p\n",current->arena,current->free,current->end);
#endif
  }
  out = current->free;
  current->free += bytes;
#if STATS
  if(copy)
    type->arenas->bytes_copied += bytes;
#endif
  return out;
}

void mark_reference(struct arenas *arenas,void *ref) {
  struct arena_type *type;
  
  type = ARENA_OF(ref)->type;
  type->grey_push(type,ref);
}

void cons_scavange(struct arena_type *type,void *ptr) {
  struct cons *cons;
  
  cons = (struct cons *)ptr;
  if(((intptr_t)cons->brooks)&1 && cons->car.p) {
    cons->car.p = evacuate(cons->car.p);
  }
  if(((intptr_t)cons->brooks)&2 && cons->cdr.p) {
    cons->cdr.p = evacuate(cons->cdr.p);
  }
}

void * cons_evacuate(struct arena_type *type,void *start) {
  struct cons *from,*to;
  
  from = (struct cons *)start;
  to = (struct cons *)arena_ensure_space(type,sizeof(struct cons),0,1);
  type->grey_push(type,to);
  memcpy(to,from,sizeof(struct cons));
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

void scavange(void *ptr) {
  struct arena_header *header;

  header = ARENA_OF(ptr);
  return header->type->scavange(header->type,ptr);
}

void ** type_grey_pull(struct arena_type *type) {
  return type->grey_pull(type);
}

void ** reg_grey_pull(struct arenas *a) {
  for(int i=0;i<NUM_REGISTERS/BITEL_BITS;i++) {
    if(a->reg_grey[i]) {
      for(int j=0;j<BITEL_BITS;j++) {
        if(a->reg_grey[i]&(1<<j)) {
          a->reg_grey[i] &= ~(1<<j);
          return &(a->registers[i*BITEL_BITS+j].r);
        }
      }
    }
  }
  return 0;
}

void * grey_pull(struct arenas *arenas) {
  void *out;
  out = type_grey_pull(&(arenas->cons_type.common));
  if(out) { return out; }
  return 0;
}

void ** rootset_pull(struct arenas *a) {
  void ** out;
  
  out = reg_grey_pull(a);
  if(out) { return out; }
  return 0;
}

int run_evacuations(struct arenas *arenas) {
  void *ptr;

  for(int i=0;i<EVACUATIONS_PER_RUN;i++) {
    ptr = (void *)grey_pull(arenas);
    if(!ptr) {
      return i>0;
    }
    if(ptr) {
      scavange(ptr);
    }
  }
  return 1;
}

void arenas_cons_init(struct arenas *arenas,struct arena_cons_type *type) {
  arenas_type_init(arenas,&(type->common),ARENA_TYPE_CODE_CONS,
                   arena_cons_alloc,cons_evacuate,cons_scavange,
                   cons_grey_push,cons_grey_pull,
                   sizeof(struct arena_cons_header));
  queue_init(&(type->grey));
}

void reg_set_p(struct arenas *arenas,int idx,void *p) {
  arenas->registers[idx].r = p;
  arenas->reg_refs[idx/BITEL_BITS] |= 1<<(idx&(BITEL_BITS-1));
  if(arenas->flags & FLAG_INGC)
    arenas->reg_grey[idx/BITEL_BITS] |= 1<<(idx&(BITEL_BITS-1));
}

void reg_set_im(struct arenas *arenas,int idx,uint32_t v) {
  arenas->registers[idx].i = v;
  arenas->reg_refs[idx/BITEL_BITS] &=~ (1<<(idx&(BITEL_BITS-1)));
  if(arenas->flags & FLAG_INGC)
    arenas->reg_grey[idx/BITEL_BITS] &=~ (1<<(idx&(BITEL_BITS-1)));
}

void * reg_get_p(struct arenas *arenas,int idx) {
  return arenas->registers[idx].r;
}

uint32_t reg_get_im(struct arenas *arenas,int idx) {
  return arenas->registers[idx].i;
}

int reg_isref(struct arenas *a,int idx) {
  return !!(a->reg_refs[idx/BITEL_BITS] & (1<<(idx&(BITEL_BITS-1))));
}

void reg_regrey(struct arenas *a,int idx) {  
  a->reg_grey[idx/BITEL_BITS] |= 1<<(idx&(BITEL_BITS-1));
}

void cons_alloc(struct arenas *arenas,int idx) {
  struct cons * out;

  out = (struct cons *)
          arena_ensure_space(&(arenas->cons_type.common),
                             sizeof(struct cons),
                             arenas->flags&FLAG_INGC,0);
  out->brooks = out;
  CONS_CAR_P_SET(out,0);
  CONS_CDR_P_SET(out,0);
  /* Put it in the specified register */
  reg_set_p(arenas,idx,out);
  reg_regrey(arenas,idx);
}

struct arenas * arenas_init() {
  struct arenas *arenas;
  arenas = malloc(sizeof(struct arenas));
  arenas->flags = 0;
  arenas_cons_init(arenas,&(arenas->cons_type));
  queue_init(&(arenas->scavange));
  for(int i=0;i<NUM_REGISTERS/BITEL_BITS;i++) {
    arenas->reg_refs[i] = 0;
    arenas->reg_grey[i] = 0;
  }
#if STATS
  arenas->bytes_copied = 0;
#endif
  return arenas;
}

#if STATS
void print_gc_stats(struct arenas * arenas) {
  char copied_u;
  int copied_v;
  
  byte_count(arenas->last_bytes_copied,&copied_v,&copied_u);
  printf("last cycle %d%c copied\n",copied_v,copied_u);
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
  
  member = (void **)rootset_pull(arenas);
  if(!member) {
    return 0;
  }
  if(*member) {
    *member = evacuate(*member);
  }
  return 1;
}

void populate_rootset(struct arenas *arenas) {
  /* Registers */
  for(int i=0;i<NUM_REGISTERS/BITEL_BITS;i++) {
    arenas->reg_grey[i] = arenas->reg_refs[i];
  }
}

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
  type->current[0].arena = 0;
}

void remark_to_as_from(struct arenas *arenas) {
  remark_type_to_as_from(&(arenas->cons_type.common));
}

void start_gc(struct arenas *arenas) {
  remark_to_as_from(arenas);
  populate_rootset(arenas);
  arenas->flags |= FLAG_INGC;
}

void finish_gc(struct arenas *arenas) {
  free_fromspace(arenas);
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
#if DEBUG
  printf("  grey queue size %d\n",queue_size(&(arenas->cons_type.grey)));
#endif
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
