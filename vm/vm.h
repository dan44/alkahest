#ifndef VM_H
#define VM_H

#include <stdint.h>

#include "queue.h"

#define ARENA_SIZE (64*1024)
#define QUEUE_FRAME_SIZE (1024*1024)

#define ARENA_OF(p) ((struct arena_header *)((intptr_t)(p)&(~(ARENA_SIZE-1))))

#define ARENA_TYPE_CODE_CONS 0

#define GENERATIONS 4

#define NUM_REGISTERS 256

#define REG_FLAGS     255

#define FLAG_ZERO     0x00000001
#define FLAG_CARRY    0x00000002

#define EVACUATIONS_PER_RUN 1000

#define FROMSPACE_MASK 0x00000001
#define GEN_MASK       0x00000006
#define GEN_SHIFT               1
#define COHORT_MASK    0x00000007
#define COHORTS        (GENERATIONS*2)

#define GEN(flags)     ((flags&GEN_MASK)>>GEN_SHIFT)
#define COHORT(g,f)    (((g)<<GEN_SHIFT)|(f))

struct arena_header;
struct arenas;

struct arena_header_list {
  struct arena_header *prev,*next;
};

struct arena_current {
  struct arena_header *arena;
  void *free,*end;
};

struct arena_type {
  uint32_t code;
  struct arenas *arenas;
  int header_size;
  struct arena_header * (members[COHORTS]);
  /* TO = 0; FROM = 1, as ever */
  /* to-space pointers for this type */
  struct arena_current current[2];
  /* virutal methods */
  struct arena_header * (*arena_alloc)(struct arenas *,int);
  void * (*evacuate)(struct arena_type *,void *);
  void   (*scavange)(struct arena_type *,void *);
  void   (*grey_push)(struct arena_type *,void *);
  void * (*grey_pull)(struct arena_type *);
};

struct arena_header {
  uint32_t flags;
  struct arena_header_list list;
  struct arena_type *type;
};

union cons_value {
  uint32_t i;
  struct cons *p;
};

struct cons {
  struct cons *brooks;
  union cons_value car,cdr;
};

struct arena_cons_type {
  struct arena_type common;
  /* grey queue */
  struct queue grey;
};

struct arena_cons_header {
  struct arena_header common;
};

union value {
  void *r;
  intptr_t i;
};

#define BITEL_BITS 32

#define FLAG_INGC 0x00000001

struct arenas {
  struct arena_cons_type cons_type;
  struct queue scavange;
  int flags;
  union value registers[NUM_REGISTERS];
  uint32_t reg_refs[NUM_REGISTERS/BITEL_BITS];
  uint32_t reg_grey[NUM_REGISTERS/BITEL_BITS];
#if STATS
  uint32_t bytes_copied,last_bytes_copied;
#endif
};

void die(int,char *);
struct arenas * arenas_init();
void arenas_destroy(struct arenas *);
void cons_alloc(struct arenas *arenas,int idx);

void reg_set_p(struct arenas *arenas,int idx,void *p);
void reg_set_im(struct arenas *arenas,int idx,uint32_t v);
void * reg_get_p(struct arenas *arenas,int idx);
intptr_t reg_get_im(struct arenas *arenas,int idx);
int reg_isref(struct arenas *a,int idx);

#if STATS
void print_gc_stats(struct arenas *);
void arenas_stats(struct arenas *);
#endif

void main_vm(); /* XXX */
void full_gc();

void * evacuate(void *);

static inline void * _write_barrier(void *from,void *to) {  
  struct arena_header *to_a;
  
  to_a = ARENA_OF(to);
  if(to_a->flags&FROMSPACE_MASK && to_a->type->arenas->flags&FLAG_INGC) { // INGC remove
    return evacuate(to);  //XXX delayed 
  }
  return to;
}

#define BROOKS(p) ((struct cons *)(((intptr_t)(p)->brooks)&~3))

#define _BITOP(c,op,b) (c)->brooks=(struct cons *)(((intptr_t)(c)->brooks) op (b))

#define CONS_CAR_P(c) (BROOKS(c)->car.p)
#define CONS_CAR_I(c) (BROOKS(c)->car.i)
#define CONS_CDR_P(c) (BROOKS(c)->cdr.p)
#define CONS_CDR_I(c) (BROOKS(c)->cdr.i)

static inline void CONS_CAR_I_SET(struct cons *c,uint32_t v) {
  struct cons * p=BROOKS(c);
  p->car.i=(v);
  _BITOP(p,&~,1);
}

static inline void CONS_CDR_I_SET(struct cons *c,uint32_t v) {
  struct cons * p=BROOKS(c);
  p->cdr.i=(v);
  _BITOP(p,&~,2);
}

static inline void CONS_CAR_P_SET(struct cons *c,void *v) {
  c = BROOKS(c);
  if(v)
    v = _write_barrier(&(c->car.p),v);
  c->car.p = v;
  _BITOP(c,|,1);
}

static inline void CONS_CDR_P_SET(struct cons *c,void *v) {
  c = BROOKS(c);
  if(v)
    v = _write_barrier(&(c->cdr.p),v);
  c->cdr.p = v;
  _BITOP(c,|,2);
}

static inline int CONS_CAR_ISREF(struct cons *c) {
  c = BROOKS(c);
  return !!(((intptr_t)(c->brooks))&1);
}

static inline int CONS_CDR_ISREF(struct cons *c) {
  c = BROOKS(c);
  return !!(((intptr_t)(c->brooks))&2);
}

#endif
