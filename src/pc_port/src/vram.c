/* The host VRAM and the libgpu transfers into, out of, and inside it. */

#include <model_tex.h>
#include <mods.h>
#include <vram.h>
#include <libgpu.h>
#include <tmd_vram.h>

#include <SDL.h>

#include <stddef.h>
#include <string.h>

uint16_t vram[VRAM_HEIGHT][VRAM_WIDTH];

static int dirty_y0 = VRAM_HEIGHT;
static int dirty_y1 = -1;
static uint16_t move_row[VRAM_WIDTH];
static VramObserver observer;
static TextureResolver resolver;

void vram_set_observer(VramObserver o) {
    observer = o;
}

void vram_set_resolver(TextureResolver r) {
    resolver = r;
}

TextureSource texture_resolve(int page_x, int page_y, int depth, int clut_x, int clut_y, int u0, int v0,
                              int u1, int v1, const void *packet) {
    unsigned model;

    if (packet != NULL) {
        model = mods_lookup_model(model_tex_lookup(packet));
        if (model != 0) {
            return (TextureSource)model;
        }
    }
    if (resolver != NULL) {
        return resolver(page_x, page_y, depth, clut_x, clut_y, u0, v0, u1, v1);
    }
    return (TextureSource)mods_lookup(page_x, page_y, u0, v0, u1, v1);
}

static void mark_dirty(int y, int h) {
    int i;
    int row;

    if (h >= VRAM_HEIGHT) {
        dirty_y0 = 0;
        dirty_y1 = VRAM_HEIGHT - 1;
        return;
    }
    for (i = 0; i < h; i++) {
        row = (y + i) & (VRAM_HEIGHT - 1);
        if (row < dirty_y0) {
            dirty_y0 = row;
        }
        if (row > dirty_y1) {
            dirty_y1 = row;
        }
    }
}

int vram_take_dirty_rows(int *y0, int *y1) {
    if (dirty_y1 < dirty_y0) {
        return 0;
    }
    *y0 = dirty_y0;
    *y1 = dirty_y1;
    dirty_y0 = VRAM_HEIGHT;
    dirty_y1 = -1;
    return 1;
}

void vram_load(int x, int y, int w, int h, const uint16_t *src) {
    int row;
    int col;

    if (w <= 0 || h <= 0) {
        return;
    }
    if (observer != NULL) {
        observer(x, y, w, h, src, VRAM_WRITE_LOAD);
    }
    for (row = 0; row < h; row++) {
        uint16_t *line = vram[(y + row) & (VRAM_HEIGHT - 1)];

        for (col = 0; col < w; col++) {
            line[(x + col) & (VRAM_WIDTH - 1)] = *src++;
        }
    }
    mark_dirty(y, h);
}

void vram_store(int x, int y, int w, int h, uint16_t *dst) {
    int row;
    int col;

    for (row = 0; row < h; row++) {
        const uint16_t *line = vram[(y + row) & (VRAM_HEIGHT - 1)];

        for (col = 0; col < w; col++) {
            *dst++ = line[(x + col) & (VRAM_WIDTH - 1)];
        }
    }
}

void vram_move(int sx, int sy, int dx, int dy, int w, int h) {
    uint16_t position[2];
    int row;
    int col;
    int r;
    int step;
    int first;

    if (w <= 0 || h <= 0) {
        return;
    }
    if (observer != NULL) {
        position[0] = (uint16_t)sx;
        position[1] = (uint16_t)sy;
        observer(dx, dy, w, h, position, VRAM_WRITE_MOVE);
    }
    /* Copy rows in the order that never reads a row already overwritten. */
    first = dy > sy ? h - 1 : 0;
    step = dy > sy ? -1 : 1;
    for (r = 0, row = first; r < h; r++, row += step) {
        const uint16_t *src = vram[(sy + row) & (VRAM_HEIGHT - 1)];
        uint16_t *dst = vram[(dy + row) & (VRAM_HEIGHT - 1)];

        for (col = 0; col < w; col++) {
            move_row[col] = src[(sx + col) & (VRAM_WIDTH - 1)];
        }
        for (col = 0; col < w; col++) {
            dst[(dx + col) & (VRAM_WIDTH - 1)] = move_row[col];
        }
    }
    mark_dirty(dy, h);
}

void vram_fill(int x, int y, int w, int h, uint16_t color) {
    int row;
    int col;

    if (w <= 0 || h <= 0) {
        return;
    }
    if (observer != NULL) {
        observer(x, y, w, h, NULL, VRAM_WRITE_CLEAR);
    }
    for (row = 0; row < h; row++) {
        uint16_t *line = vram[(y + row) & (VRAM_HEIGHT - 1)];

        for (col = 0; col < w; col++) {
            line[(x + col) & (VRAM_WIDTH - 1)] = color;
        }
    }
    mark_dirty(y, h);
}

uint16_t vram_color15(unsigned r, unsigned g, unsigned b) {
    return (uint16_t)(((b >> 3) << 10) | ((g >> 3) << 5) | (r >> 3));
}

int LoadImage(RECT *rect, u_long *p) {
    if (rect->w == 14 && rect->h == 1 && (rect->y == 242 || rect->y == 246)) {
        SDL_Log("picture_load hook: rect(x:%d, y:%d) -> ponteiro p = %p", rect->x, rect->y, (void *)p);
    }
    vram_load(rect->x, rect->y, rect->w, rect->h, (const uint16_t *)p);
    tmd_vram_note_rect(0, rect->x, rect->y, rect->w, rect->h);
    return 0;
}

int StoreImage(RECT *rect, u_long *p) {
    gpu_refresh_draw_overlap(rect->x, rect->y, rect->w, rect->h);
    vram_store(rect->x, rect->y, rect->w, rect->h, (uint16_t *)p);
    return 0;
}

int MoveImage(RECT *rect, int x, int y) {
    int capture;

    capture = gpu_album_prepare(rect->x, rect->y, x, y, rect->w, rect->h);
    vram_move(rect->x, rect->y, x, y, rect->w, rect->h);
    if (capture) {
        gpu_album_note(rect->x, rect->y, x, y, rect->w, rect->h);
    }
    tmd_vram_note_rect(1, x, y, rect->w, rect->h);
    return 0;
}

int MoveImage2(RECT *rect, int x, int y) {
    return MoveImage(rect, x, y);
}

int ClearImage(RECT *rect, u_char r, u_char g, u_char b) {
    vram_fill(rect->x, rect->y, rect->w, rect->h, vram_color15(r, g, b));
    tmd_vram_note_rect(2, rect->x, rect->y, rect->w, rect->h);
    if (rect->x < 320 && rect->y < 480 && rect->x + rect->w > 0 && rect->y + rect->h > 0) {
        gpu_clear_screen(r, g, b);
    }
    return 0;
}

int ClearImage2(RECT *rect, u_char r, u_char g, u_char b) {
    return ClearImage(rect, r, g, b);
}
