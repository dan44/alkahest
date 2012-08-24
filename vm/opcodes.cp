#include <stdio.h>
#include <stdlib.h>

#include "vm.h"
#include "util.h"
#include "opcodes.h"

void o_reg_assign(struct arenas *a,int to,int from) {
  a->registers[to] = a->registers[from];
  BITSET(a->reg_refs,to,BITGET(a->reg_refs,from));
}

void o_reg_nil(struct arenas *a,int reg) {
  a->registers[reg].r = 0;
  BITSET(a->reg_refs,reg,1);
}

#define OPCODE_BYTE0(x) opcode_##x

static inline void INVALID_OPCODE(uint32_t x) {
  fprintf(stderr,"Invalid opcode %2.2X\n",x);
  exit(135);
}

#define J(x) &&opcode_##x - &&start
#define NEXT(op) goto *(&&start + jumps[op]);

void execute(struct arenas *a,uint32_t *pc) {
  static int jumps[] = {
##jumptable
  };
  pc--;
  start:
    NEXT(*(++pc)>>24);
##opcode 0x80 /* HALT */
    return;
    
##opcode 0xC0 /* REGNIL */
  o_reg_nil(a,OPCODEC_R(*pc));
  NEXT((*(++pc))>>24);

##opcoderest
}
