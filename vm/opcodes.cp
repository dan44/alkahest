#include <stdio.h>
#include <stdlib.h>

#include "vm.h"
#include "util.h"
#include "opcodes.h"
#include "cons.h"

##readcodes opcodes.dat

void o_reg_assign(struct arenas *a,int to,int from) {
  a->registers[to] = a->registers[from];
  BITSET(a->reg_refs,to,BITGET(a->reg_refs,from));
  // TODO: write barrier?
}

void o_reg_nil(struct arenas *a,int reg) {
  a->registers[reg].r = 0;
  BITSET(a->reg_refs,reg,1);
}

void o_reg_im(struct arenas *a,int reg,uint32_t val) {
  a->registers[reg].i = val;
  BITSET(a->reg_refs,reg,0);
}

void o_reg_ref(struct arenas *a,int reg,void * val) {
  a->registers[reg].r = val;
  BITSET(a->reg_refs,reg,1);
}

static inline struct cons * chase_cons(struct cons *c,uint8_t path) {
  while(path) {
    if(path&2) {
      c = CONS_CAR_P(c);
    } else if(path&1) {
      c = CONS_CDR_P(c);
    }
    path >>= 2;
  }
  return c;
}

// TODO: dedup code
static inline void o_regcxrreg(struct arenas *a,uint32_t reg_dest,
                               uint8_t path,uint32_t reg_src) {
  struct cons *c;
  
  c=reg_get_p(a,reg_src);
  while(path&~3) {
    if(path&2) {
      c = CONS_CAR_P(c);
    } else if(path&1) {
      c = CONS_CDR_P(c);
    }
    path >>= 2;
  }
  if(path&2) {
    if(CONS_CAR_ISREF(c)) {
      reg_set_p(a,reg_dest,CONS_CAR_P(c));
    } else {
      reg_set_im(a,reg_dest,CONS_CAR_I(c));
    }
  } else if(path&1) {
    if(CONS_CDR_ISREF(c)) {
      reg_set_p(a,reg_dest,CONS_CDR_P(c));
    } else {
      reg_set_im(a,reg_dest,CONS_CDR_I(c));
    }
  } else {
    o_reg_assign(a,reg_dest,reg_src);
  } 
}

static inline void o_carsetreg(struct arenas *a,uint32_t reg_dest,
                               uint8_t path,uint32_t reg_src) {
  struct cons *c;
  
  if(path) {
    c=reg_get_p(a,reg_src);
    while(path&~3) {
      if(path&2) {
        c = CONS_CAR_P(c);
      } else if(path&1) {
        c = CONS_CDR_P(c);
      }
      path >>= 2;
    }
    if(path&2) {
      if(CONS_CAR_ISREF(c)) {
        CONS_CAR_P_SET(reg_get_p(a,reg_dest),CONS_CAR_P(c));
      } else {
        CONS_CAR_I_SET(reg_get_p(a,reg_dest),CONS_CAR_I(c));
      }
    } else if(path&1) {
      if(CONS_CDR_ISREF(c)) {
        CONS_CAR_P_SET(reg_get_p(a,reg_dest),CONS_CDR_P(c));
      } else {
        CONS_CAR_I_SET(reg_get_p(a,reg_dest),CONS_CDR_I(c));
      }
    }
  } else {
    if(reg_isref(a,reg_src)) {
      CONS_CAR_P_SET(reg_get_p(a,reg_dest),reg_get_p(a,reg_src));
    } else {
      CONS_CAR_I_SET(reg_get_p(a,reg_dest),reg_get_im(a,reg_src));
    }
  }
}

static inline void o_cdrsetreg(struct arenas *a,uint32_t reg_dest,
                               uint8_t path,uint32_t reg_src) {
  struct cons *c;
  
  if(path) {
    c=reg_get_p(a,reg_src);
    while(path&~3) {
      if(path&2) {
        c = CONS_CAR_P(c);
      } else if(path&1) {
        c = CONS_CDR_P(c);
      }
      path >>= 2;
    }
    if(path&2) {
      if(CONS_CAR_ISREF(c)) {
        CONS_CDR_P_SET(reg_get_p(a,reg_dest),CONS_CAR_P(c));
      } else {
        CONS_CDR_I_SET(reg_get_p(a,reg_dest),CONS_CAR_I(c));
      }
    } else if(path&1) {
      if(CONS_CDR_ISREF(c)) {
        CONS_CDR_P_SET(reg_get_p(a,reg_dest),CONS_CDR_P(c));
      } else {
        CONS_CDR_I_SET(reg_get_p(a,reg_dest),CONS_CDR_I(c));
      }
    }
  } else {
    if(reg_isref(a,reg_src)) {
      CONS_CDR_P_SET(reg_get_p(a,reg_dest),reg_get_p(a,reg_src));
    } else {
      CONS_CDR_I_SET(reg_get_p(a,reg_dest),reg_get_im(a,reg_src));
    }
  }
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
  uint32_t v1,v2,v3;
  intptr_t p1;
  //uint32_t *orig_pc = pc; // XXX for debugging
  
  /* Initialize */
  o_reg_im(a,REG_FLAGS,0);
  
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

##opcode CARSETNIL
  CONS_CAR_P_SET(chase_cons(reg_get_p(a,C_C(*pc)),C_B(*pc)),0);
  NEXT(*(++pc));  

##opcode CDRSETNIL
  CONS_CDR_P_SET(chase_cons(reg_get_p(a,C_C(*pc)),C_B(*pc)),0);
  NEXT(*(++pc));  

##opcode CARSETREG
  o_carsetreg(a,C_A(*pc),C_B(*pc),C_C(*pc));
  NEXT(*(++pc));  

##opcode CDRSETREG
  o_cdrsetreg(a,C_A(*pc),C_B(*pc),C_C(*pc));
  NEXT(*(++pc));  

##opcode CARSETVAL
  CONS_CAR_I_SET(chase_cons(reg_get_p(a,C_C(*pc)),C_B(*pc)),*(pc+1));
  ++pc;
  NEXT(*(++pc));  

##opcode CDRSETVAL
  CONS_CDR_I_SET(chase_cons(reg_get_p(a,C_C(*pc)),C_B(*pc)),*(pc+1));
  ++pc;
  NEXT(*(++pc));  

##opcode REGCXRREG
  o_regcxrreg(a,C_A(*pc),C_B(*pc),C_C(*pc));
  NEXT(*(++pc));  

##opcode CMPREGNIL
  v1 = reg_get_im(a,C_C(*pc));
  v3 = reg_get_im(a,REG_FLAGS);
  if(v1) {
    v3 &=~ (FLAG_ZERO|FLAG_CARRY);
  } else {
    v3 |= FLAG_ZERO|FLAG_CARRY;
  }
  reg_set_im(a,REG_FLAGS,v3);  
  NEXT(*(++pc));  

##opcode CONS
  cons_alloc(a,C_C(*pc));
  NEXT(*(++pc));  

##opcode MVREG
  o_reg_assign(a,C_B(*pc),C_C(*pc));
  NEXT(*(++pc));

##opcode CMPREGREGIM
  v1 = reg_get_im(a,C_B(*pc));
  v2 = reg_get_im(a,C_C(*pc));
  v3 = reg_get_im(a,REG_FLAGS);
  if(v1 == v2) {
    v3 |= FLAG_ZERO|FLAG_CARRY;
  } else {
    v3 &=~ FLAG_ZERO;
    if(v1 > v2) {
      v3 |= FLAG_CARRY;
    } else {
      v3 &= FLAG_CARRY;
    }      
  }
  reg_set_im(a,REG_FLAGS,v3);
  NEXT(*(++pc));

##opcode J
  val = (int32_t)*(++pc);
  pc += val;
  NEXT(*(++pc));

##opcode JZ
  v1 = reg_get_im(a,REG_FLAGS);
  val = (int32_t)*(++pc);
  if(v1&FLAG_ZERO) {
    pc += val;
  }
  NEXT(*(++pc));

##opcode JNZ
  v1 = reg_get_im(a,REG_FLAGS);
  val = (int32_t)*(++pc);
  if(!(v1&FLAG_ZERO)) {
    pc += val;
  }
  NEXT(*(++pc));

##opcode CALL
  reg = C_C(*(pc++));
  p1 = (intptr_t)(pc + *pc + 1);
  reg_set_im(a,reg,(intptr_t)++pc);
  pc = (uint32_t *)p1;
  NEXT(*pc);

##opcode RET
  pc = (uint32_t *)reg_get_im(a,C_C(*pc));
  NEXT(*pc);  

##opcode invalid
  INVALID_OPCODE(*pc);
}
