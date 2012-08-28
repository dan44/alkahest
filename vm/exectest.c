#include <stdio.h>
#include <stdlib.h>

#include "vm.h"
#include "opcodes.h"
#include "exectest_aa.h"

int main(int argc,char **argv) {
  struct arenas *arenas;
  
  arenas = arenas_init();
  execute(arenas,aa_exectest);
  arenas_destroy(arenas);
  return 1;
}
