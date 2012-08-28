#include <stdio.h>
#include <stdlib.h>

#include "vm.h"
#include "util.h"
#include "opcodes.h"

##readcodes vm/opcodes.dat

void o_reg_assign(struct arenas *a,int to,int from) {
  a->registers[to] = a->registers[from];
  BITSET(a->reg_refs,to,BITGET(a->reg_refs,from));
  // TODO: write barrier?
}

static inline void o_reg_nil(struct arenas *a,int reg) {
  a->registers[reg].r = 0;
  BITSET(a->reg_refs,reg,1);
}

static inline void o_reg_im(struct arenas *a,int reg,uint32_t val) {
  a->registers[reg].i = val;
  BITSET(a->reg_refs,reg,0);
}

#define OPCODE_BYTE0(x) opcode_##x

static inline void INVALID_OPCODE(uint32_t x) {
  fprintf(stderr,"Invalid opcode %2.2X\n",x);
  exit(135);
}

#define C_I(x) ((x)>>24)
#define C_A(x) (((x)>>16)&0xFF)
#define C_B(x) (((x)>>8)&0xFF)
#define C_C(x) ((x)&0xFF)

#define J(x) &&opcode_##x
#define NEXT(op) goto *(jumps[C_I(op)]);

void execute(struct arenas *a,uint32_t *pc) {
  int reg,val;
  void * jumps[] = {
##jumptable
  };
  
  NEXT(*pc);

##opcode HALT
    return;

##opcode NOP
  NEXT(*(++pc));
    
##opcode ALARM
  printf("ALARM!\n");
  NEXT(*(++pc));
    
##opcode REGNIL
  o_reg_nil(a,C_C(*pc));
  NEXT(*(++pc));

##opcode SETIM
  reg = C_C(*pc);
  val = *(++pc);
  o_reg_im(a,reg,val);
  NEXT(*(++pc));

##opcode DJNZ
  reg = C_C(*pc);
  val = (int32_t)*(++pc);
  a->registers[reg].i--;
  if(a->registers[reg].i) {
    pc += val;
  }
  NEXT(*(++pc));  

##opcode invalid
  INVALID_OPCODE(*pc);
}
