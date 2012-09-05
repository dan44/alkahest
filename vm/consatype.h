#ifndef CONSATYPE_H
#define CONSATYPE_H

struct arena_cons_type {
  struct arena_type common;
  /* grey queue */
  struct queue grey;
};

void arenas_cons_init(struct arenas *arenas,struct arena_cons_type *type);
void arenas_cons_destroy(struct arena_cons_type *type);

#endif

