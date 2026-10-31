#ifndef PS1_MEMORY_H
#define PS1_MEMORY_H

#include <stdint.h>

/* The PlayStation addresses the game stores through, mapped at the same host addresses. */
#define PS1_RAM_BASE 0x80000000u
#define PS1_RAM_SIZE 0x00200000u
#define PS1_SCRATCHPAD_BASE 0x1F800000u
#define PS1_SCRATCHPAD_SIZE 0x00000400u

/* 24-bit ordering-table links. Below PS1_LINK_RAM_END is an offset into PS1 RAM;
 * PS1_LINK_IMAGE_START up to PS1_LINK_IMAGE_END is a host image address. */
#define PS1_LINK_END 0x00FFFFFFu
#define PS1_LINK_RAM_END 0x00200000u
#define PS1_LINK_IMAGE_START 0x00400000u
#define PS1_LINK_IMAGE_END 0x01000000u

/* Maps PS1 RAM and the scratchpad. Returns 0, or -1 after logging the range that failed. */
int ps1_memory_map(void);

/* The address a 24-bit link names, or NULL for the terminator, zero, or a value
 * outside both windows. */
void *ps1_link_to_ptr(uint32_t link);
uint32_t ps1_ptr_to_link(const void *p);

/* Logs the code and address of an unhandled fault before the process ends. */
void host_install_crash_logger(void);

/* Diagnostic thread: if *vblank_counter stops for 2 s, logs where the main thread is.
 * It never touches game state. */
void host_start_watchdog(const int *vblank_counter);

/* Startup checks for the mapping, the overlay symbols, and the link rule.
 * Returns 0 when all pass. */
int ps1_memory_self_check(void);

#endif
