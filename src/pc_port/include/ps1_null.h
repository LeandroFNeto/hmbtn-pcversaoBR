#ifndef PS1_NULL_H
#define PS1_NULL_H

/* Reads and writes through a pointer that is null on the PS1 too. There, address 0 is
 * kernel RAM and the access goes on; the host reaches the same RAM through its KSEG0
 * window, which ps1_memory_map seeds with the kernel's values. Game sources reach this
 * only through host patches (cmake/host.cmake). */

#include <ps1_memory.h>

#define PS1_NULL_PTR(p)                                                                       \
    ({                                                                                        \
        __typeof__(p) ps1_null_p = (p);                                                       \
        ps1_null_p != 0 ? ps1_null_p : (__typeof__(p))(uintptr_t)PS1_RAM_BASE;                \
    })

#endif
