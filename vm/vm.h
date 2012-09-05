#ifndef VM_H
#define VM_H

#include <stdlib.h>
#include <stdint.h>

#include "queue.h"

#include "vmconfig.h"
#include "arenatypes.h"
#include "consatype.h"

#define HOWMANY(s,t) ((s)/sizeof(t))

struct arena_header;
struct arenas;

union value {
  void *r;
  intptr_t i;
};

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

void reg_set_p(struct arenas *arenas,int idx,void *p);
void reg_set_im(struct arenas *arenas,int idx,uint32_t v);
void * reg_get_p(struct arenas *arenas,int idx);
intptr_t reg_get_im(struct arenas *arenas,int idx);
int reg_isref(struct arenas *a,int idx);

void * arena_alloc();
void arena_init(struct arena_header *header,struct arena_type *type,int fromspace);
void * arena_ensure_space(struct arena_type *type,size_t bytes,int from,int copy);

void arenas_type_init(struct arenas * arenas,struct arena_type *type,
                      uint32_t code,
                      struct arena_header * (*arena_alloc)(struct arenas *,int),
                      void * (*evacuate)(struct arena_type *,void *),
                      void (*scavange)(struct arena_type *,void *),
                      void (*grey_push)(struct arena_type *,void *),
                      void * (*grey_pull)(struct arena_type *),
                      size_t header_size);
void reg_regrey(struct arenas *a,int idx);
void arenas_type_destroy(struct arena_type *type,int to);


#if STATS
void print_gc_stats(struct arenas *);
void arenas_stats(struct arenas *);
#endif

void main_vm(); /* XXX */
void full_gc();

static inline void * _write_barrier(void *from,void *to) {  
  struct arena_header *to_a;
  
  to_a = ARENA_OF(to);
  if(to_a->flags&FROMSPACE_MASK && to_a->type->arenas->flags&FLAG_INGC) { // INGC remove
    return evacuate(to);  //XXX delayed 
  }
  return to;
}

#endif
