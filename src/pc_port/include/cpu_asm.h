#ifndef CPU_ASM_H
#define CPU_ASM_H

/* Host replacement for src/include/cpu_asm.h. The scratchpad addresses stay; the
 * stack switch, return-address store, and debugger break have no host equivalent. */

#define SCRATCHPAD          0x1F800000
#define SCRATCHPAD_SIZE     0x400
#define SCRATCHPAD_STACK_TOP (SCRATCHPAD + SCRATCHPAD_SIZE - 4)

#define SCRATCHPAD_STACK_ENTER() do { } while (0)
#define SCRATCHPAD_STACK_LEAVE() do { } while (0)
#define SAVE_RETURN_ADDRESS(where) do { (void)(where); } while (0)
#define pollhost() do { } while (0)

#endif
