#include <stdio.h>
#include <stdlib.h>

#include "vm.h"
#include "opcodes.h"

void add_list(struct arenas *arenas) {
  /* r1 -- current list start & list whose car is matching pair for v; out
   * r2 -- newly allocated pair & current cons pair considered
   * r5 -- previous search position (for extending)
   * r6 -- value
   * r7 -- scratch
   */

  uint32_t code_frag_c[] = {
    I_REGNIL(5),
    I_HALT,
  };
  
  uint32_t code_frag_a[] = {
    I_CARSETREG(1,0x00,2),
    
    I_REGCXRREG(5,0x02,1),
    I_CARSETREG(5,0x00,6),
    
    I_CDRSETNIL(0x02,1),
    I_CDRSETNIL(0x00,1),
    I_REGNIL(2),
    I_REGNIL(5),
    I_HALT,
  };

  uint32_t code_frag_f[] = {
    I_CDRSETREG(5,0x00,1),
    I_HALT,
  };
     
  execute(arenas,code_frag_c);
  if(reg_get_p(arenas,1)) {
    for(o_reg_assign(arenas,2,1);
        reg_get_p(arenas,2);
        reg_set_p(arenas,2,CONS_CDR_P((struct cons *)reg_get_p(arenas,2)))) {
      if(CONS_CAR_I(CONS_CAR_P((struct cons *)reg_get_p(arenas,2))) == reg_get_im(arenas,6)) {
        o_reg_assign(arenas,1,2);
        return;
      }
      o_reg_assign(arenas,5,2);
    }
  }
  cons_alloc(arenas,1);
  cons_alloc(arenas,2);
  if(reg_get_p(arenas,5)) {
    execute(arenas,code_frag_f);
  }
  execute(arenas,code_frag_a);
}

void read_word(struct arenas *arenas,char *line) {
   /* r0 -- root of tree; in and out
    * r1 -- list whose car is a matching pair for this char
    * r2 -- used as scratch in callees
    * r3 -- place r4 is rooted in tree in CDR of pair, or 0 if root
    * r4 -- current linear list of chars (or tail thereof) at this level
    * r5 -- used as scratch in callees
    * r6 -- passed as val to callees
    */

  uint32_t code_frag_b[] = {
    I_REGNIL(3),
    I_REGNIL(4),
    I_HALT,
  };

  uint32_t code_frag_d[] = {
    I_REGNIL(3),
    I_HALT,
  };

  uint32_t code_frag_e[] = {
    I_REGNIL(1),
    I_HALT,
  };

  o_reg_assign(arenas,4,0);
  execute(arenas,code_frag_d);  
  for(char *c=line;*c;c++) {
    o_reg_assign(arenas,1,4);
    o_reg_im(arenas,6,(char)*c);
    add_list(arenas);
    if(!reg_get_p(arenas,0)) {
      o_reg_assign(arenas,0,1);
    } else if(!reg_get_p(arenas,4)) {
      CONS_CDR_P_SET(reg_get_p(arenas,3),reg_get_p(arenas,1));
    }
    reg_set_p(arenas,3,CONS_CAR_P((struct cons *)reg_get_p(arenas,1)));
    reg_set_p(arenas,4,CONS_CDR_P((struct cons *)reg_get_p(arenas,3)));
    execute(arenas,code_frag_e);  
  }
  execute(arenas,code_frag_b);
}

struct cons * read_words(struct arenas *arenas) {
  FILE *in;
  char line[128];
  
  if(!(in = fopen("/usr/share/dict/words","r"))) {
    die(129,"open failed");
  }
  reg_set_p(arenas,0,0);
  while(fgets(line,128,in)) {
    read_word(arenas,line);
  }
  fclose(in);
  return reg_get_p(arenas,0);
}

void dump_words(struct cons * here,char *acc,int depth) {
  for(struct cons * x=here;x;x=CONS_CDR_P(x)) {
    acc[depth] = (char)CONS_CAR_I(CONS_CAR_P(x));
    if(CONS_CDR_P(CONS_CAR_P(x))) {
      dump_words(CONS_CDR_P(CONS_CAR_P(x)),acc,depth+1);
    } else {
      acc[depth+1]='\0';
      printf("%s",acc);
    }
  }
}

int main(int argc,char **argv) {
  struct arenas *arenas;
  struct cons *root;
  char buffer[128];
  
  arenas = arenas_init();
  root = read_words(arenas);
  arenas->registers[0].r = root;
  arenas->reg_refs[0] = 1;
  main_vm(arenas);
  for(int i=0;i<5;i++)
    full_gc(arenas);
  dump_words(arenas->registers[0].r,buffer,0);
  arenas->registers[0].r = 0;
  arenas->reg_refs[0] = 1;
  full_gc(arenas);
#if STATS
  arenas_stats(arenas);
#endif
  //arenas_destroy(arenas);
  return 1;
}
