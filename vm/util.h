#ifndef UTIL_H
#define UTIL_H

#include <stdint.h>

/* TODO: find some assembler */
static inline int BITGET(uint32_t *from,int idx) {
  return !!(*(from+(idx>>5)) & (1<<(idx&31)));
}

static inline void BITSET(uint32_t *to,int idx,int val) {
  if(val) {
    *(to+(idx>>5)) = (*(to+(idx>>5))) | (1<<(idx&31));
  } else {
    *(to+(idx>>5)) = (*(to+(idx>>5))) & ~(1<<(idx&31));
  }
}

uint64_t now_usec();
void die(int,char *);
char * make_message(const char *, ...);
void byte_count(uint64_t,int *,char *);

#endif

