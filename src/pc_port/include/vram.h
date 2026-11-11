#ifndef PC_PORT_VRAM_H
#define PC_PORT_VRAM_H

#include <stdint.h>

/* The PS1 VRAM: 1024 x 512 16-bit words, written exactly as the game writes them
 * (5-5-5-1 pixels, packed 4-bit and 8-bit texel indices, CLUT rows). */

#define VRAM_WIDTH 1024
#define VRAM_HEIGHT 512

extern uint16_t vram[VRAM_HEIGHT][VRAM_WIDTH];

typedef enum {
    VRAM_WRITE_LOAD,
    VRAM_WRITE_CLEAR,
    VRAM_WRITE_MOVE
} VramWriteKind;

/* Every write is reported here before it lands. `source` is the CPU data for a load,
 * the source VRAM position (x, y as two uint16_t) for a move, NULL for a clear. */
typedef void (*VramObserver)(int x, int y, int w, int h, const void *source, VramWriteKind kind);
void vram_set_observer(VramObserver observer);

/* Transfers, with PS1 wrapping at 1024 x 512. */
void vram_load(int x, int y, int w, int h, const uint16_t *src);
void vram_store(int x, int y, int w, int h, uint16_t *dst);
void vram_move(int sx, int sy, int dx, int dy, int w, int h);
void vram_fill(int x, int y, int w, int h, uint16_t color);
uint16_t vram_color15(unsigned r, unsigned g, unsigned b);

/* Rows written since the last call; returns 0 when nothing changed. */
int vram_take_dirty_rows(int *y0, int *y1);

/* What a textured primitive samples. A /mods backend replaces texture_resolve. */
typedef enum {
    TEXTURE_FROM_VRAM = 0
} TextureSource;
typedef TextureSource (*TextureResolver)(int page_x, int page_y, int depth, int clut_x, int clut_y,
                                         int u0, int v0, int u1, int v1);
void vram_set_resolver(TextureResolver resolver);
TextureSource texture_resolve(int page_x, int page_y, int depth, int clut_x, int clut_y, int u0, int v0,
                              int u1, int v1, const void *packet);

#endif
