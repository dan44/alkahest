#include <stdio.h>
#include <stdlib.h>

#include "vm.h"
#include "opcodes.h"

int main(int argc,char **argv) {
  struct arenas *arenas;
  uint32_t program[] = {
    0xC1000000, 0x40000000, /* SETIM 0 0x40000000 */
    0x82000000,             /* ALARM */
    0xC2000000, 0xFFFFFFFD, /* DJNZ 0 -3 */
    0x80000000,             /* HALT */
  };
  
  arenas = arenas_init();
  execute(arenas,program);
  full_gc(arenas);
  arenas_destroy(arenas);
  return 1;
}
