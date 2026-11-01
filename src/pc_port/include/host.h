#ifndef PC_PORT_HOST_H
#define PC_PORT_HOST_H

#include <setjmp.h>

/* The window and context main_pc.c created; host_present swaps them. */
struct SDL_Window;
extern struct SDL_Window *host_window;

/* main_pc.c sets this before entering the game loop; host_quit jumps back to it. */
extern jmp_buf host_exit_jmp;
void host_quit(void);

/* Sends SDL_Log lines to `path`, flushed per line, so they survive a killed process. */
void host_log_open(const char *path);

/* Swaps the window, then clears the back buffer for the next vblank. */
void host_present(void);
void host_clear(void);

/* Draws one vblank's content without presenting: the VSync callback, then the green
 * quad while the game has never drawn. Returns 1 when the frame has something to
 * present; a vblank without a game draw keeps the previous frame on screen. */
int host_render_vblank(void);
int host_game_has_drawn(void);
/* For the frame checks: forget that a check's callback drew. */
void host_debug_reset_game_drawn(void);
int host_vblank_count(void);
const int *host_vblank_counter(void);

/* 1 when HMBTN_TRACE=1: scene changes and memory card calls go to the log. */
int host_tracing(void);

/* Frame-loop checks for HMBTN_SELFTEST=1. Returns 0 when all pass. */
int host_frame_self_check(void);

#endif
