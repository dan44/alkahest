#ifndef VMCONFIG_H
#define VMCONFIG_H

#define ARENA_SIZE (64*1024)
#define QUEUE_FRAME_SIZE (1024*1024)

#define ARENA_TYPE_CODE_CONS 0

#define GENERATIONS 4

#define NUM_REGISTERS 256

#define REG_FLAGS     255

#define FLAG_ZERO     0x00000001
#define FLAG_CARRY    0x00000002

#define EVACUATIONS_PER_RUN 1000

#define FROMSPACE_MASK 0x00000001
#define GEN_MASK       0x00000006
#define GEN_SHIFT               1
#define COHORT_MASK    0x00000007
#define COHORTS        (GENERATIONS*2)

#define GEN(flags)     ((flags&GEN_MASK)>>GEN_SHIFT)
#define COHORT(g,f)    (((g)<<GEN_SHIFT)|(f))

#define BITEL_BITS 32

#define FLAG_INGC 0x00000001

#endif
