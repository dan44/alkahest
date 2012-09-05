#include <stdio.h>
#include <stdlib.h>

#include "vm.h"
#include "opcodes.h"
#include "cons.h"
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
