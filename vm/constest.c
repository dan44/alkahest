#include <stdio.h>
#include <stdlib.h>

#include "vm.h"

struct cons * add_list(struct arenas *arenas,struct cons *start,uint32_t v) {
  struct cons *p = 0;
  struct cons *out;

  if(start) {
    for(struct cons *c=start;c;c = CONS_CDR_P(c)) {
      if(CONS_CAR_I(CONS_CAR_P(c)) == v) {
        return c;
      }
      p = c;
    }
  }
  out = cons_alloc(arenas);
  if(p) {
    CONS_CDR_P_SET(p,out);
  }
  CONS_CAR_P_SET(out,cons_alloc(arenas));
  CONS_CAR_I_SET(CONS_CAR_P(out),v);
  CONS_CDR_P_SET(CONS_CAR_P(out),0);
  CONS_CDR_P_SET(out,0);
  return out;
}

void read_word(struct arenas *arenas,struct cons **root,char *line) {
  struct cons * node,*next,*p;

  next = *root;
  node = 0;
  for(char *c=line;*c;c++) {
    p = add_list(arenas,next,(char)*c);
    if(!*root) {
      *root = p;
    } else if(!next) {
      CONS_CDR_P_SET(node,p);
    }
    node = CONS_CAR_P(p);
    next = CONS_CDR_P(node);
  }
}

struct cons * read_words(struct arenas *arenas) {
  FILE *in;
  char line[128];
  struct cons *out;
  
  if(!(in = fopen("/usr/share/dict/words","r"))) {
    die(129,"open failed");
  }
  out = 0;
  while(fgets(line,128,in)) {
    read_word(arenas,&out,line);
  }
  fclose(in);
  return out;
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
  for(int i=0;i<50;i++)
    full_gc(arenas);
  dump_words(arenas->registers[0].r,buffer,0);
  arenas->registers[0].r = 0;
  arenas->reg_refs[0] = 1;
  full_gc(arenas);
#if STATS
  arenas_stats(arenas);
#endif
  arenas_destroy(arenas);
  return 1;
}
