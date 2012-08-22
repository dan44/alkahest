#ifndef VM_H
#define VM_H

#define STATS 1

#include <stdint.h>

#define ARENA_SIZE (64*1024)
#define QUEUE_FRAME_SIZE (64*1024)

#define ARENA_TYPE_CODE_CONS 0

#define GENERATIONS 4

#define NUM_REGISTERS 256

#define EVACUATIONS_PER_RUN 1000

struct arena_header;
struct arenas;

struct queue {
  struct queue_frame *head,*tail;
};

struct queue_frame {
  struct queue_frame *next;
  void **head,**tail,**end;
};

struct arena_header_list {
  struct arena_header *prev,*next;
};

struct arena_type {
  uint32_t code;
  struct arenas *arenas;
  struct arena_header *from;
  int header_size;
  struct arena_header * (members[GENERATIONS][2]);
  /* to-space pointers for this type */
  struct arena_header *to;
  void *free,*end;
  /* virutal methods */
  struct arena_header * (*arena_alloc)(struct arenas *,int);
  void * (*evacuate)(struct arena_type *,void *);
};

#define FROMSPACE_MASK 0x00000001
#define GEN_MASK       0x00000006
#define GEN_SHIFT               1

#define GEN(flags)     ((flags&GEN_MASK)>>GEN_SHIFT)

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
};

struct arena_cons_header {
  struct arena_header common;
  struct cons *free,*end;
};

union value {
  void *r;
  intptr_t i;
};

#define BITEL_BITS 32

#define FLAG_INGC 0x00000001

struct arenas {
  struct arena_cons_type cons_type;
  struct queue grey,rootset;
  int flags;
  union value registers[NUM_REGISTERS];
  uint32_t reg_refs[NUM_REGISTERS/BITEL_BITS];
#if STATS
  uint32_t bytes_copied,last_bytes_copied;
#endif
};

void die(int,char *);
struct arenas * arenas_init();
void arenas_destroy(struct arenas *);

struct cons * cons_alloc(struct arenas *);

#if STATS
void print_gc_stats(struct arenas *);
#endif

void main_vm(); /* XXX */

#define BROOKS(p) ((struct cons *)(((intptr_t)(p)->brooks)&~3))

#define _BITOP(c,op,b) (c)->brooks=(struct cons *)(((intptr_t)(c)->brooks) op (b))

#define CONS_CAR_P(c) (BROOKS(c)->car.p)
#define CONS_CAR_I(c) (BROOKS(c)->car.i)
#define CONS_CDR_P(c) (BROOKS(c)->cdr.p)
#define CONS_CDR_I(c) (BROOKS(c)->cdr.i)
#define CONS_CAR_P_SET(c,v) do { BROOKS(c)->car.p=(v); _BITOP(BROOKS(c),|,1); } while(0)
#define CONS_CAR_I_SET(c,v) do { BROOKS(c)->car.i=(v); _BITOP(BROOKS(c),&~,1); } while(0)
#define CONS_CDR_P_SET(c,v) do { BROOKS(c)->cdr.p=(v); _BITOP(BROOKS(c),|,2); } while(0)
#define CONS_CDR_I_SET(c,v) do { BROOKS(c)->cdr.i=(v); _BITOP(BROOKS(c),&~,2); } while(0)

#endif
