#include <stdio.h>
#include <stdlib.h>

#include "vm.h"

void add_list(struct arenas *arenas,uint32_t v) {
  /* r1 -- current list start & list whose car is matching pair for v; out
   * r2 -- newly allocated pair & current cons pair considered
   * r5 -- previous search position (for extending)
   */
  
  reg_set_p(arenas,5,0);
  if(reg_get_p(arenas,1)) {
    for(reg_set_p(arenas,2,reg_get_p(arenas,1));
        reg_get_p(arenas,2);
        reg_set_p(arenas,2,CONS_CDR_P((struct cons *)reg_get_p(arenas,2)))) {
      if(CONS_CAR_I(CONS_CAR_P((struct cons *)reg_get_p(arenas,2))) == v) {
        reg_set_p(arenas,1,reg_get_p(arenas,2));
        return;
      }
      reg_set_p(arenas,5,reg_get_p(arenas,2));
    }
  }
  cons_alloc(arenas,1);
  cons_alloc(arenas,2);
  if(reg_get_p(arenas,5)) {
    CONS_CDR_P_SET(reg_get_p(arenas,5),reg_get_p(arenas,1));
  }
  CONS_CAR_P_SET(reg_get_p(arenas,1),reg_get_p(arenas,2));
  CONS_CAR_I_SET(CONS_CAR_P((struct cons *)reg_get_p(arenas,1)),v);
  CONS_CDR_P_SET(CONS_CAR_P((struct cons *)reg_get_p(arenas,1)),0);
  CONS_CDR_P_SET(reg_get_p(arenas,1),0);
  reg_set_p(arenas,2,0);
  reg_set_p(arenas,5,0);
}

void read_word(struct arenas *arenas,char *line) {
   /* r0 -- root of tree; in and out
    * r1 -- list whose car is a matching pair for this char
    * r2 -- used as scratch in callees
    * r3 -- place r4 is rooted in tree in CDR of pair, or 0 if root
    * r4 -- current linear list of chars (or tail thereof) at this level
    * r5 -- used as scratch in callees
    */
  reg_set_p(arenas,4,reg_get_p(arenas,0));
  reg_set_p(arenas,3,0);
  for(char *c=line;*c;c++) {
    reg_set_p(arenas,1,reg_get_p(arenas,4));
    add_list(arenas,(char)*c);
    if(!reg_get_p(arenas,0)) {
      reg_set_p(arenas,0,reg_get_p(arenas,1));
    } else if(!reg_get_p(arenas,4)) {
      CONS_CDR_P_SET(reg_get_p(arenas,3),reg_get_p(arenas,1));
    }
    reg_set_p(arenas,3,CONS_CAR_P((struct cons *)reg_get_p(arenas,1)));
    reg_set_p(arenas,4,CONS_CDR_P((struct cons *)reg_get_p(arenas,3)));
    reg_set_p(arenas,1,0);
  }
  reg_set_p(arenas,3,0);
  reg_set_p(arenas,4,0);
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
