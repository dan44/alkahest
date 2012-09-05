#include <string.h>

#include "vm.h"
#include "cons.h"

struct arena_cons_header {
  struct arena_header common;
};

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

void cons_grey_push(struct arena_type *type,void *ref) {
  queue_push(&(((struct arena_cons_type *)type)->grey),ref);
}

void * cons_grey_pull(struct arena_type *type) {
  return queue_pull(&(((struct arena_cons_type *)type)->grey));
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

void arenas_cons_init(struct arenas *arenas,struct arena_cons_type *type) {
  arenas_type_init(arenas,&(type->common),ARENA_TYPE_CODE_CONS,
                   arena_cons_alloc,cons_evacuate,cons_scavange,
                   cons_grey_push,cons_grey_pull,
                   sizeof(struct arena_cons_header));
  queue_init(&(type->grey));
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

void arenas_cons_destroy(struct arena_cons_type *type) {
  arenas_type_destroy(&(type->common),1);
  queue_destroy(&(type->grey));
}

