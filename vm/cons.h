#ifndef CONS_H
#define CONS_H

#include "arenatypes.h"

/* PRIVATE */
/* Do not inspect/alter these structs, use the macros and inlines below */
union cons_value {
  uint32_t i;
  struct cons *p;
};

struct cons {
  struct cons *brooks;
  union cons_value car,cdr;
};

void cons_alloc(struct arenas *arenas,int idx);

#define BROOKS(p) ((struct cons *)(((intptr_t)(p)->brooks)&~3))

#define _BITOP(c,op,b) (c)->brooks=(struct cons *)(((intptr_t)(c)->brooks) op (b))

/* PUBLIC from here */

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
