/* Startup checks for the PS1 address space and the 24-bit link rule. */

#include <ps1_memory.h>
#include <ps1_null.h>

#include <SDL.h>
#include <libgpu.h>

#include <string.h>

extern unsigned char overlay_low[];
extern unsigned char overlay_high[];

static int check(int ok, const char *what) {
    if (!ok) {
        SDL_Log("PS1 memory check failed: %s", what);
    }
    return ok ? 0 : 1;
}

/* The kernel's words in low RAM, and null pointers leading to them. */
static int check_low_ram(void) {
    volatile const uint32_t *low = (volatile const uint32_t *)(uintptr_t)PS1_RAM_BASE;
    uint32_t *none = NULL;
    uint32_t word = 0;
    int failed;

    failed = check(low[0] == 0x00000003u && low[1] == 0x275A0C80u && low[2] == 0x03400008u && low[3] == 0,
                   "0x80000000..0x8000000F hold the kernel's garbage area");
    failed |= check(low[0x60 / 4] == 2, "0x80000060 holds the RAM size, 2");
    failed |= check((uintptr_t)PS1_NULL_PTR(none) == PS1_RAM_BASE, "a null pointer leads to 0x80000000");
    failed |= check(PS1_NULL_PTR(&word) == &word, "a non-null pointer passes through");
    if (!failed) {
        SDL_Log("PS1 memory check ok: low RAM holds the kernel's values, null pointers lead to it");
    }
    return failed;
}

int ps1_memory_self_check(void) {
    volatile unsigned char *work;
    volatile uint32_t *spad;
    POLY_F4 *quad;
    u_long head;
    u_long *table;
    int failed;
    int i;

    failed = 0;

    work = (volatile unsigned char *)0x801BE1B0u;
    *work = 0x5A;
    failed |= check(*work == 0x5A, "write/read 0x801BE1B0");
    *work = 0;
    failed |= check(*(volatile unsigned char *)0x801F7000u == 0, "0x801F7000 starts zero");
    spad = (volatile uint32_t *)0x1F8003FCu;
    *spad = 0x12345678u;
    failed |= check(*spad == 0x12345678u, "write/read 0x1F8003FC");
    *spad = 0;

    failed |= check((uintptr_t)overlay_low == 0x800B7E20u, "overlay_low is 0x800B7E20");
    failed |= check((uintptr_t)overlay_high == 0x8012DDE8u, "overlay_high is 0x8012DDE8");

    /* A flat quad in PS1 RAM, linked by its 24-bit offset, ended by the terminator. */
    quad = (POLY_F4 *)0x801C0000u;
    memset(quad, 0, sizeof(*quad));
    setPolyF4(quad);
    setaddr(quad, PS1_LINK_END);
    setXY4(quad, 10, 10, 20, 10, 10, 20, 20, 20);
    head = 0;
    setaddr(&head, quad);
    failed |= check(getaddr(&head) == 0x001C0000u, "PS1 RAM packet links as 0x001C0000");
    failed |= check(gpu_debug_walk(&head) == 1, "walk draws the PS1 RAM packet and stops at the terminator");
    failed |= check(gpu_debug_last_steps() == 2, "walk stops after the terminator tag");
    memset(quad, 0, sizeof(*quad));

    /* A link outside both windows ends the walk without reading it. */
    head = 0x00300000u;
    failed |= check(gpu_debug_walk(&head) == 0, "walk stops at 0x00300000");
    failed |= check(gpu_debug_last_steps() == 1, "walk does not step through 0x00300000");

    /* An 8-entry reverse table in PS1 RAM walks to its terminator and draws nothing. */
    table = (u_long *)0x801C1000u;
    ClearOTagR(table, 8);
    failed |= check(getaddr(&table[0]) == PS1_LINK_END, "ClearOTagR terminates entry 0");
    failed |= check(getaddr(&table[7]) == ps1_ptr_to_link(&table[6]), "ClearOTagR links entry 7 to 6");
    failed |= check(gpu_debug_walk(&table[7]) == 0, "walk of a cleared table draws nothing");
    failed |= check(gpu_debug_last_steps() == 8, "walk of a cleared table visits 8 entries");
    for (i = 0; i < 8; i++) {
        table[i] = 0;
    }

    failed |= check_low_ram();
    return failed;
}
