#ifndef OPCODES_H
#define OPCODES_H

/* Opcode Format A:
 * <-8-><-8-><-8-><-8->
 * opcd dest src1 src2
 * 
 * Opcode Format B:
 * <---16---><-8-><-8->
 *   opcd    dest src1
 * 
 * Opcode Format C:
 * <-----24-----><-8->
 * opcode        reg
 */
 
/* Opcodes Format A */
#define OPCODE_HALT   0x80000000
#define OPCODE_NOP    0x81000000


/* Opcodes Format B */
#define OPCODE_MVREG  0x00000000

#define OPCODEB(c,d,s) ((c)|((a)<<8)|(b))

/* Opcodes Format C */
#define OPCODE_REGNIL 0xC0000000

#define OPCODE_SETIM  0xC1000000

#define OPCODEC(c,r) ((c)|(r))
#define OPCODEC_R(c) ((c)&0xFF)

void o_reg_assign(struct arenas *,int,int);
void execute(struct arenas *a,uint32_t *code);

#endif
