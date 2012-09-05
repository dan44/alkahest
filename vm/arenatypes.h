#ifndef ARENATYPES_H
#define ARENATYPES_H

#include "vmconfig.h"

#define ARENA_OF(p) ((struct arena_header *)((intptr_t)(p)&(~(ARENA_SIZE-1))))

struct arena_header_list {
  struct arena_header *prev,*next;
};

struct arena_header {
  uint32_t flags;
  struct arena_header_list list;
  struct arena_type *type;
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

void * evacuate(void *); // XXX move impl somewhere sensible

#endif
