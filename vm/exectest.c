#include <stdio.h>
#include <stdlib.h>

#include "vm.h"
#include "opcodes.h"

int main(int argc,char **argv) {
  struct arenas *arenas;
  uint32_t program[] = {
    I_SETIM(0,0x40000000),
    I_NOP,
    I_DJNZ(0,0xFFFFFFFD),
    I_HALT,
  };
  
  arenas = arenas_init();
  execute(arenas,program);
  full_gc(arenas);
  arenas_destroy(arenas);
  return 1;
}
