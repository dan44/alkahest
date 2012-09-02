#include <stdio.h>
#include <stdlib.h>

#include "vm.h"
#include "opcodes.h"
#include "constest_aa.h"

void word_to_list(struct arenas *a,char *line) {
  /* r8 -- result
   * r5,r7 -- scratch
   */
  for(char *c=line;*c;c++) {
    cons_alloc(a,7);
    CONS_CAR_I_SET(reg_get_p(a,7),(uint32_t)*c);
    CONS_CDR_P_SET(reg_get_p(a,7),0);    
    if(reg_get_p(a,5)) {
      CONS_CDR_P_SET(reg_get_p(a,5),reg_get_p(a,7));
      reg_set_p(a,5,reg_get_p(a,7));
    } else {
      reg_set_p(a,5,reg_get_p(a,7));
      reg_set_p(a,8,reg_get_p(a,7));      
    }
  }
}

#if 0
void add_list(struct arenas *arenas) {
  /* r1 -- current list start & list whose car is matching pair for v; out
   * r2 -- newly allocated pair & current cons pair considered
   * r5 -- previous search position (for extending)
   * r6 -- value
   * r7 -- scratch
   */
  execute(arenas,aa_constest);
}

void read_word(struct arenas *arenas,char *line) {
   /* r0 -- root of tree; in and out
    * r1 -- list whose car is a matching pair for this char
    * r2 -- used as scratch in callees
    * r3 -- place r4 is rooted in tree in CDR of pair, or 0 if root
    * r4 -- current linear list of chars (or tail thereof) at this level
    * r5,r7 -- used as scratch in callees and locally
    * r6 -- passed as val to callees
    * r8 -- word in
    */

  uint32_t code_frag_b[] = {
    I_REGNIL(3),
    I_REGNIL(4),
    I_HALT,
  };

  uint32_t code_frag_d[] = {
    I_MVREG(4,0),
    I_REGNIL(3),
    I_HALT,
  };

  uint32_t code_frag_f[] = {
    I_REGISNOTNIL(0,3),
    I_MVREG(0,1),
    I_J(3),
    I_REGISNOTNIL(4,1),
    I_CDRSETREG(3,0x00,1),
    I_REGCXRREG(3,0x02,1),
    I_REGCXRREG(4,0x01,3),
    I_REGNIL(1),
    I_REGCXRREG(8,0x01,8),
    I_HALT,
  };

  uint32_t code_frag_g[] = {
    I_MVREG(1,4),
    I_REGCXRREG(6,0x02,8),    
    I_HALT,
  };

  word_to_list(arenas,line);
  execute(arenas,code_frag_d);  
  
  while(reg_get_p(arenas,8)) {
    execute(arenas,code_frag_g);
    add_list(arenas);
    execute(arenas,code_frag_f);
  }
  execute(arenas,code_frag_b);
}
#endif

void read_word(struct arenas *arenas,char *line) {
  word_to_list(arenas,line);
  execute(arenas,aa_constest);
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
