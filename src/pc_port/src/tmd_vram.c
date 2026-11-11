/* Rectangles copied into VRAM since the farm scene was entered, and one frame of
 * the textured primitives that sample word 0x0000 over a black blob. */

#include <tmd_vram.h>

#include <host.h>
#include <libgpu.h>
#include <model_tex.h>
#include <vram.h>

#include <SDL.h>

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int main_scene;

#define NOTE_MAX 2048
#define RECT_CAP_MAX 65536
#define LOG_MAX 16384

enum { RECT_LOAD = 0, RECT_MOVE = 1, RECT_CLEAR = 2 };

typedef struct RectRec {
    int kind;
    int x, y, w, h;
} RectRec;

typedef struct Note {
    unsigned cmd;
    short x[4];
    short y[4];
    int n;
    int textured;
    int u, v;
    int page_x, page_y, clut_x, clut_y, depth;
    const void *model;
} Note;

static RectRec *rects;
static int rect_n;
static int rect_head;
static int rect_cap;
static int rect_overflow;
static Note *notes;
static int note_n;
static int note_overflow;
static int *blob_queue;
static char *log_text;
static int log_len;
static int debug_on;
static int force_enabled = -1;
static int f8_was;
static int dumped;
static int dump_n;
static int farm_was;

static int env_enabled(void) {
    static int cached = -1;
    const char *env;

    if (cached < 0) {
        env = getenv("HMBTN_TMD_VRAM");
        cached = env != NULL && strcmp(env, "1") == 0;
    }
    return cached;
}

static int on_farm(void) {
    return main_scene == 2 || main_scene == 6 || main_scene == 14 || main_scene == 15 || main_scene == 19 ||
           main_scene == 20;
}

static int recording(void) {
    if (debug_on || force_enabled >= 0) {
        return 1;
    }
    return env_enabled() && on_farm();
}

int tmd_vram_collecting(void) {
    return recording();
}

static int ensure(void) {
    if (rects == NULL) {
        rect_cap = RECT_CAP_MAX;
        rects = (RectRec *)calloc((size_t)rect_cap, sizeof(RectRec));
    }
    if (notes == NULL) {
        notes = (Note *)calloc((size_t)NOTE_MAX, sizeof(Note));
    }
    if (blob_queue == NULL) {
        blob_queue = (int *)malloc(60 * 80 * sizeof(int));
    }
    if (log_text == NULL) {
        log_text = (char *)malloc(LOG_MAX);
        if (log_text != NULL) {
            log_text[0] = '\0';
        }
    }
    return rects != NULL && notes != NULL && blob_queue != NULL && log_text != NULL;
}

static void add_line(const char *fmt, ...) {
    char buf[384];
    va_list ap;
    int n;

    va_start(ap, fmt);
    n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (n < 0) {
        return;
    }
    SDL_Log("%s", buf);
    if (log_text == NULL || log_len + n + 2 >= LOG_MAX) {
        return;
    }
    memcpy(log_text + log_len, buf, (size_t)n);
    log_len += n;
    log_text[log_len++] = '\n';
    log_text[log_len] = '\0';
}

static void reset_notes(void) {
    note_n = 0;
    note_overflow = 0;
}

static const char *kind_name(int kind) {
    if (kind == RECT_MOVE) {
        return "move";
    }
    if (kind == RECT_CLEAR) {
        return "clear";
    }
    return "load";
}

static int covers(const RectRec *rect, int x, int y) {
    return x >= rect->x && y >= rect->y && x < rect->x + rect->w && y < rect->y + rect->h;
}

static const RectRec *rect_newest(int age) {
    int i;

    i = rect_head - 1 - age;
    if (i < 0) {
        i += rect_cap;
    }
    return &rects[i];
}

static void log_cover(int x, int y) {
    int i;

    for (i = 0; i < rect_n; i++) {
        const RectRec *rect;

        rect = rect_newest(i);
        if (covers(rect, x, y)) {
            add_line("tmdvram: cover %d,%d rect %s x %d y %d w %d h %d", x, y, kind_name(rect->kind), rect->x, rect->y,
                     rect->w, rect->h);
            return;
        }
    }
    add_line("tmdvram: cover %d,%d rect none", x, y);
}

void tmd_vram_note_rect(int kind, int x, int y, int w, int h) {
    int now;

    if (w <= 0 || h <= 0 || !ensure()) {
        return;
    }
    now = on_farm();
    if (!debug_on && force_enabled < 0) {
        if (now && !farm_was) {
            rect_n = 0;
            rect_head = 0;
            rect_overflow = 0;
        }
        farm_was = now;
    }
    if (!recording()) {
        return;
    }
    if (rect_cap <= 0) {
        rect_overflow = 1;
        return;
    }
    rects[rect_head].kind = kind;
    rects[rect_head].x = x;
    rects[rect_head].y = y;
    rects[rect_head].w = w;
    rects[rect_head].h = h;
    rect_head++;
    if (rect_head >= rect_cap) {
        rect_head = 0;
    }
    if (rect_n < rect_cap) {
        rect_n++;
    }
}

void tmd_vram_note_drawn(unsigned cmd, int n, const short *xy, int textured, int u, int v, int page_x, int page_y,
                         int clut_x, int clut_y, int depth, const void *packet) {
    Note *note;
    int i;

    if (!tmd_vram_collecting() || !ensure()) {
        return;
    }
    if (note_n >= NOTE_MAX) {
        note_overflow = 1;
        return;
    }
    note = &notes[note_n++];
    memset(note, 0, sizeof(*note));
    note->cmd = cmd;
    note->n = n > 4 ? 4 : n;
    note->textured = textured;
    note->u = u;
    note->v = v;
    note->page_x = page_x;
    note->page_y = page_y;
    note->clut_x = clut_x;
    note->clut_y = clut_y;
    note->depth = depth;
    note->model = model_tex_lookup(packet);
    for (i = 0; i < note->n; i++) {
        note->x[i] = xy[i * 2];
        note->y[i] = xy[i * 2 + 1];
    }
}

static int overlaps(const Note *note, int x0, int y0, int x1, int y1) {
    int i;
    int minx;
    int miny;
    int maxx;
    int maxy;

    if (note->n <= 0) {
        return 0;
    }
    minx = maxx = note->x[0];
    miny = maxy = note->y[0];
    for (i = 1; i < note->n; i++) {
        if (note->x[i] < minx) {
            minx = note->x[i];
        }
        if (note->x[i] > maxx) {
            maxx = note->x[i];
        }
        if (note->y[i] < miny) {
            miny = note->y[i];
        }
        if (note->y[i] > maxy) {
            maxy = note->y[i];
        }
    }
    return minx < x1 && maxx > x0 && miny < y1 && maxy > y0;
}

static int covers_point(const Note *note, int x, int y) {
    int i;
    int minx;
    int miny;
    int maxx;
    int maxy;

    if (note->n <= 0) {
        return 0;
    }
    minx = maxx = note->x[0];
    miny = maxy = note->y[0];
    for (i = 1; i < note->n; i++) {
        if (note->x[i] < minx) {
            minx = note->x[i];
        }
        if (note->x[i] > maxx) {
            maxx = note->x[i];
        }
        if (note->y[i] < miny) {
            miny = note->y[i];
        }
        if (note->y[i] > maxy) {
            maxy = note->y[i];
        }
    }
    return x >= minx && x <= maxx && y >= miny && y <= maxy;
}

static void sample_parts(const Note *note, unsigned *raw, int *index, unsigned *color) {
    int tu;
    int tv;
    int x;
    int y;
    unsigned word;

    *raw = 0;
    *index = -1;
    *color = 0;
    if (!note->textured) {
        return;
    }
    tu = note->u;
    tv = note->v;
    if (note->depth <= 4) {
        x = note->page_x + tu / 4;
        y = note->page_y + tv;
        word = vram[y & 511][x & 1023];
        *raw = word;
        *index = (int)((word >> ((tu & 3) * 4)) & 0xF);
        *color = vram[note->clut_y & 511][(note->clut_x + *index) & 1023];
        return;
    }
    if (note->depth <= 8) {
        x = note->page_x + tu / 2;
        y = note->page_y + tv;
        word = vram[y & 511][x & 1023];
        *raw = word;
        *index = (tu & 1) ? (int)((word >> 8) & 0xFF) : (int)(word & 0xFF);
        *color = vram[note->clut_y & 511][(note->clut_x + *index) & 1023];
        return;
    }
    word = vram[(note->page_y + tv) & 511][(note->page_x + tu) & 1023];
    *raw = word;
    *color = word;
}

static void rect_text(const Note *note, char *buf, int cap) {
    int x;
    int y;
    int i;

    if (note->depth <= 4) {
        x = note->page_x + note->u / 4;
        y = note->page_y + note->v;
    } else if (note->depth <= 8) {
        x = note->page_x + note->u / 2;
        y = note->page_y + note->v;
    } else {
        x = note->page_x + note->u;
        y = note->page_y + note->v;
    }
    x &= 1023;
    y &= 511;
    for (i = 0; i < rect_n; i++) {
        const RectRec *rect;

        rect = rect_newest(i);
        if (covers(rect, x, y)) {
            snprintf(buf, (size_t)cap, "rect %s x %d y %d w %d h %d", kind_name(rect->kind), rect->x, rect->y, rect->w,
                     rect->h);
            return;
        }
    }
    snprintf(buf, (size_t)cap, "rect none");
}

static int largest_blob(const unsigned char grid[60][80], int *x0, int *y0, int *x1, int *y1) {
    unsigned char seen[60][80];
    int y;
    int x;
    int best;
    int best_x0, best_y0, best_x1, best_y1;

    memset(seen, 0, sizeof(seen));
    best = 0;
    best_x0 = best_y0 = best_x1 = best_y1 = 0;
    for (y = 0; y < 60; y++) {
        for (x = 0; x < 80; x++) {
            int qh;
            int qt;
            int n;
            int minx;
            int miny;
            int maxx;
            int maxy;

            if (!grid[y][x] || seen[y][x]) {
                continue;
            }
            qh = 0;
            qt = 0;
            n = 0;
            minx = maxx = x;
            miny = maxy = y;
            blob_queue[qt++] = y * 80 + x;
            seen[y][x] = 1;
            while (qh < qt) {
                int cur;
                int cx0;
                int cy0;
                static const int dx[4] = {-1, 1, 0, 0};
                static const int dy[4] = {0, 0, -1, 1};
                int k;

                cur = blob_queue[qh++];
                cx0 = cur % 80;
                cy0 = cur / 80;
                n++;
                if (cx0 < minx) {
                    minx = cx0;
                }
                if (cx0 > maxx) {
                    maxx = cx0;
                }
                if (cy0 < miny) {
                    miny = cy0;
                }
                if (cy0 > maxy) {
                    maxy = cy0;
                }
                for (k = 0; k < 4; k++) {
                    int nx;
                    int ny;

                    nx = cx0 + dx[k];
                    ny = cy0 + dy[k];
                    if (nx < 0 || ny < 0 || nx >= 80 || ny >= 60 || !grid[ny][nx] || seen[ny][nx]) {
                        continue;
                    }
                    seen[ny][nx] = 1;
                    blob_queue[qt++] = ny * 80 + nx;
                }
            }
            if (n > best) {
                best = n;
                best_x0 = minx * 4;
                best_y0 = miny * 4;
                best_x1 = (maxx + 1) * 4;
                best_y1 = (maxy + 1) * 4;
            }
        }
    }
    *x0 = best_x0;
    *y0 = best_y0;
    *x1 = best_x1;
    *y1 = best_y1;
    return best;
}

static int word_is_zero(const Note *note, unsigned *raw, int *index, unsigned *color) {
    if (!note->textured) {
        return 0;
    }
    sample_parts(note, raw, index, color);
    return *raw == 0 || *color == 0;
}

static void log_sample(const Note *note, unsigned raw, int index, unsigned color) {
    char rect[96];

    rect_text(note, rect, (int)sizeof(rect));
    if (index < 0) {
        add_line("tmdvram: sample cmd 0x%02x page %d,%d uv %d,%d clut %d,%d raw 0x%04x index none color 0x%04x %s",
                 note->cmd, note->page_x, note->page_y, note->u, note->v, note->clut_x, note->clut_y, raw, color, rect);
    } else {
        add_line("tmdvram: sample cmd 0x%02x page %d,%d uv %d,%d clut %d,%d raw 0x%04x index %d color 0x%04x %s",
                 note->cmd, note->page_x, note->page_y, note->u, note->v, note->clut_x, note->clut_y, raw, index, color,
                 rect);
    }
}

#define MAP_MAX 512

static const unsigned char MAP_COLOR[][3] = {
    {255, 40, 40}, {40, 255, 40}, {255, 220, 40}, {40, 220, 255}, {255, 40, 220}, {255, 140, 40}, {240, 240, 240},
    {80, 80, 255},
};

static const unsigned char MAP_DIGIT[10][5] = {
    {0x7, 0x5, 0x5, 0x5, 0x7}, {0x2, 0x6, 0x2, 0x2, 0x7}, {0x7, 0x1, 0x7, 0x4, 0x7}, {0x7, 0x1, 0x7, 0x1, 0x7},
    {0x5, 0x5, 0x7, 0x1, 0x1}, {0x7, 0x4, 0x7, 0x1, 0x7}, {0x7, 0x4, 0x7, 0x5, 0x7}, {0x7, 0x1, 0x1, 0x1, 0x1},
    {0x7, 0x5, 0x7, 0x5, 0x7}, {0x7, 0x5, 0x7, 0x1, 0x7},
};

static void map_put(unsigned char *px, int w, int h, int x, int y, int r, int g, int b) {
    int i;

    if (x < 0 || y < 0 || x >= w || y >= h) {
        return;
    }
    i = (y * w + x) * 4;
    px[i] = (unsigned char)b;
    px[i + 1] = (unsigned char)g;
    px[i + 2] = (unsigned char)r;
}

static void map_line(unsigned char *px, int w, int h, int x0, int y0, int x1, int y1, int r, int g, int b) {
    int dx;
    int dy;
    int sx;
    int sy;
    int err;
    int i;

    dx = x1 - x0;
    dy = y1 - y0;
    if (dx < 0) {
        dx = -dx;
    }
    if (dy < 0) {
        dy = -dy;
    }
    sx = x0 < x1 ? 1 : -1;
    sy = y0 < y1 ? 1 : -1;
    err = dx - dy;
    for (i = 0; i < w + h; i++) {
        int e2;

        map_put(px, w, h, x0, y0, r, g, b);
        map_put(px, w, h, x0 + 1, y0, r, g, b);
        if (x0 == x1 && y0 == y1) {
            break;
        }
        e2 = err * 2;
        if (e2 > -dy) {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y0 += sy;
        }
    }
}

static int map_sx(int x, int w) {
    int sx;

    sx = (int)(((float)x + 0.5f) * (float)w / 320.0f);
    if (sx < 0) {
        sx = 0;
    }
    if (sx >= w) {
        sx = w - 1;
    }
    return sx;
}

static int map_sy(int y, int h) {
    int top;
    int sy;

    top = (int)(((float)y + 0.5f) * (float)h / 240.0f);
    sy = h - 1 - top;
    if (sy < 0) {
        sy = 0;
    }
    if (sy >= h) {
        sy = h - 1;
    }
    return sy;
}

static void map_digit(unsigned char *px, int w, int h, int x, int y, int digit, int r, int g, int b) {
    int row;
    int col;

    if (digit < 0 || digit > 9) {
        return;
    }
    for (row = 0; row < 5; row++) {
        for (col = 0; col < 3; col++) {
            if ((MAP_DIGIT[digit][row] & (4 >> col)) == 0) {
                continue;
            }
            map_put(px, w, h, x + col * 2, y + row * 2, r, g, b);
            map_put(px, w, h, x + col * 2 + 1, y + row * 2, r, g, b);
            map_put(px, w, h, x + col * 2, y + row * 2 + 1, r, g, b);
            map_put(px, w, h, x + col * 2 + 1, y + row * 2 + 1, r, g, b);
        }
    }
}

static void map_number(unsigned char *px, int w, int h, int x, int y, int number, int r, int g, int b) {
    if (number >= 10) {
        map_digit(px, w, h, x, y, number / 10, r, g, b);
        map_digit(px, w, h, x + 8, y, number % 10, r, g, b);
    } else {
        map_digit(px, w, h, x, y, number, r, g, b);
    }
}

static void map_edge(unsigned char *px, int w, int h, const Note *note, int a, int b, int r, int g, int bl) {
    if (a >= note->n || b >= note->n) {
        return;
    }
    map_line(px, w, h, map_sx(note->x[a], w), map_sy(note->y[a], h), map_sx(note->x[b], w), map_sy(note->y[b], h), r, g,
             bl);
}

static unsigned char *map_load(const char *path, int *w, int *h) {
    FILE *in;
    unsigned char header[54];
    int vw;
    int vh;
    int bpp;
    unsigned char *pixels;

    in = fopen(path, "rb");
    if (in == NULL) {
        return NULL;
    }
    if (fread(header, 1, sizeof header, in) != sizeof header || header[0] != 'B' || header[1] != 'M') {
        fclose(in);
        return NULL;
    }
    memcpy(&vw, header + 18, 4);
    memcpy(&vh, header + 22, 4);
    bpp = header[28] | (header[29] << 8);
    if (bpp != 32 || vw < 1 || vh < 1 || vw > 2048 || vh > 2048) {
        fclose(in);
        return NULL;
    }
    pixels = (unsigned char *)malloc((size_t)vw * (size_t)vh * 4u);
    if (pixels == NULL || fread(pixels, 4, (size_t)vw * (size_t)vh, in) != (size_t)vw * (size_t)vh) {
        free(pixels);
        fclose(in);
        return NULL;
    }
    fclose(in);
    *w = vw;
    *h = vh;
    return pixels;
}

static int map_save(const char *path, const unsigned char *pixels, int w, int h) {
    FILE *out;
    unsigned char header[54];
    uint32_t size;

    out = fopen(path, "wb");
    if (out == NULL) {
        return 1;
    }
    memset(header, 0, sizeof header);
    header[0] = 'B';
    header[1] = 'M';
    size = 54u + (uint32_t)w * (uint32_t)h * 4u;
    memcpy(header + 2, &size, 4);
    header[10] = 54;
    header[14] = 40;
    memcpy(header + 18, &w, 4);
    memcpy(header + 22, &h, 4);
    header[26] = 1;
    header[28] = 32;
    fwrite(header, 1, sizeof header, out);
    fwrite(pixels, 4, (size_t)w * (size_t)h, out);
    fclose(out);
    return 0;
}

static void write_poly_map(const int *ids, int n, int x0, int y0, int x1, int y1) {
    int index;
    char photo[32];
    char image[40];
    char list_path[40];
    unsigned char *pixels;
    int w;
    int h;
    int made;
    FILE *list;
    int i;

    index = host_f8_index();
    if (index < 1 || n <= 0) {
        return;
    }
    snprintf(photo, sizeof photo, "f8-%03d.bmp", index);
    snprintf(image, sizeof image, "f8-%03d-map.bmp", index);
    snprintf(list_path, sizeof list_path, "f8-%03d-map.txt", index);
    pixels = map_load(photo, &w, &h);
    made = 0;
    if (pixels == NULL) {
        w = 320;
        h = 240;
        pixels = (unsigned char *)calloc((size_t)w * (size_t)h, 4u);
        made = 1;
    }
    if (pixels == NULL) {
        add_line("tmdvram: map save-failed");
        return;
    }
    list = fopen(list_path, "w");
    if (list != NULL) {
        fprintf(list, "blob %d %d %d %d\n", x0, y0, x1, y1);
    }
    for (i = 0; i < n && i < MAP_MAX; i++) {
        const Note *note;
        int color;
        int r;
        int g;
        int b;
        int nx;
        int ny;

        note = &notes[ids[i]];
        color = i % 8;
        r = MAP_COLOR[color][0];
        g = MAP_COLOR[color][1];
        b = MAP_COLOR[color][2];
        if (note->n >= 4) {
            map_edge(pixels, w, h, note, 0, 1, r, g, b);
            map_edge(pixels, w, h, note, 1, 3, r, g, b);
            map_edge(pixels, w, h, note, 3, 2, r, g, b);
            map_edge(pixels, w, h, note, 2, 0, r, g, b);
        } else {
            map_edge(pixels, w, h, note, 0, 1, r, g, b);
            map_edge(pixels, w, h, note, 1, 2, r, g, b);
            map_edge(pixels, w, h, note, 2, 0, r, g, b);
        }
        nx = map_sx(note->x[0], w);
        ny = map_sy(note->y[0], h) - 12;
        if (ny < 0) {
            ny = 0;
        }
        map_number(pixels, w, h, nx, ny, i, r, g, b);
        if (list != NULL) {
            fprintf(list, "poly %d color %d,%d,%d page %d,%d uv %d,%d model ", i, r, g, b, note->page_x, note->page_y,
                    note->u, note->v);
            if (note->model == NULL) {
                fprintf(list, "none");
            } else {
                fprintf(list, "tmd_%08x", (unsigned)(uintptr_t)note->model);
            }
            fprintf(list, " xy");
            for (color = 0; color < note->n; color++) {
                fprintf(list, " %d,%d", note->x[color], note->y[color]);
            }
            fprintf(list, "\n");
        }
    }
    if (list != NULL) {
        fclose(list);
    }
    if (map_save(image, pixels, w, h) != 0) {
        add_line("tmdvram: map save-failed");
    } else {
        add_line("tmdvram: map %s polys %d", image, n < MAP_MAX ? n : MAP_MAX);
    }
    free(pixels);
}

/* 1 when a dump was written. A full-screen untextured polygon is a light or a
 * sepia overlay and is not a reason to skip. An opaque fill that owns the
 * black blob is not a reason to drop another primitive that samples 0x0000.
 * A full-height blob dumps the center of the view instead of its outline. */
static int capture(void) {
    unsigned char grid[60][80];
    int x0, y0, x1, y1;
    int cells;
    int i;
    int use_center;
    int full_height;
    int hit;
    int ignored;
    int shown[MAP_MAX];
    int shown_n;

    if (!ensure()) {
        return 0;
    }
    if (!gpu_fill_black_grid(grid)) {
        add_line("tmdvram: skip no-grid");
        reset_notes();
        return 0;
    }
    cells = largest_blob(grid, &x0, &y0, &x1, &y1);
    full_height = cells > 0 && y1 - y0 >= 240;
    use_center = cells == 0 || full_height;
    hit = 0;
    shown_n = 0;
    for (i = 0; i < note_n; i++) {
        Note *note;
        unsigned raw;
        unsigned color;
        int index;

        note = &notes[i];
        if (use_center) {
            if (!covers_point(note, 160, 120)) {
                continue;
            }
        } else if (!overlaps(note, x0, y0, x1, y1)) {
            continue;
        }
        if (!word_is_zero(note, &raw, &index, &color)) {
            continue;
        }
        if (!hit) {
            dump_n++;
            add_line("tmdvram: dump %d", dump_n);
            if (full_height) {
                add_line("tmdvram: blob full-height x0 %d y0 %d x1 %d y1 %d center 160,120", x0, y0, x1, y1);
            } else if (use_center) {
                add_line("tmdvram: blob none center 160,120");
            } else {
                add_line("tmdvram: blob x0 %d y0 %d x1 %d y1 %d", x0, y0, x1, y1);
            }
        }
        hit = 1;
        if (shown_n < MAP_MAX) {
            shown[shown_n++] = i;
        }
        log_sample(note, raw, index, color);
    }
    ignored = 0;
    if (!hit && !use_center && cells > 0) {
        for (i = 0; i < note_n; i++) {
            Note *note;
            unsigned raw;
            unsigned color;
            int index;

            note = &notes[i];
            if (!word_is_zero(note, &raw, &index, &color)) {
                continue;
            }
            if (!ignored) {
                dump_n++;
                add_line("tmdvram: dump %d", dump_n);
                add_line("tmdvram: blob clear ignored x0 %d y0 %d x1 %d y1 %d", x0, y0, x1, y1);
                ignored = 1;
            }
            hit = 1;
            if (shown_n < MAP_MAX) {
                shown[shown_n++] = i;
            }
            log_sample(note, raw, index, color);
        }
    }
    if (!hit) {
        add_line(use_center ? "tmdvram: skip center none" : "tmdvram: skip blob clear");
        reset_notes();
        return 0;
    }
    if (note_overflow) {
        add_line("tmdvram: notes overflow");
    }
    if (rect_overflow) {
        add_line("tmdvram: rects overflow");
    }
    if (!debug_on) {
        write_poly_map(shown, shown_n, x0, y0, x1, y1);
    }
    reset_notes();
    return 1;
}

void tmd_vram_present(int f8_down) {
    int rising;

    if (debug_on) {
        return;
    }
    if (!env_enabled() && force_enabled < 0) {
        f8_was = f8_down ? 1 : 0;
        return;
    }
    rising = f8_down && !f8_was;
    f8_was = f8_down ? 1 : 0;
    if (!rising) {
        reset_notes();
        return;
    }
    if (force_enabled < 0 && !on_farm()) {
        add_line("tmdvram: skip off-farm scene %d", main_scene);
        reset_notes();
        return;
    }
    capture();
}

static void debug_begin(void) {
    debug_on = 1;
    ensure();
    log_len = 0;
    if (log_text != NULL) {
        log_text[0] = '\0';
    }
    rect_n = 0;
    rect_head = 0;
    rect_overflow = 0;
    reset_notes();
}

static const char *debug_log(void) {
    return log_text != NULL ? log_text : "";
}

static int check(int ok, const char *what) {
    SDL_Log("tmdvram check %s: %s", ok ? "ok" : "FAILED", what);
    return ok ? 0 : 1;
}

static void draw_sprite_over(int x0, int y0, int x1, int y1) {
    static POLY_F4 black;
    static SPRT sprt;
    static DR_TPAGE tpage;

    memset(&tpage, 0, sizeof(tpage));
    setDrawTPage(&tpage, 0, 0, getTPage(2, 0, 640, 256));
    termPrim(&tpage);
    DrawOTag((u_long *)&tpage);

    memset(&black, 0, sizeof(black));
    setPolyF4(&black);
    setRGB0(&black, 0, 0, 0);
    setXY4(&black, x0, y0, x1, y0, x0, y1, x1, y1);
    memset(&sprt, 0, sizeof(sprt));
    setSprt(&sprt);
    setRGB0(&sprt, 128, 128, 128);
    setXY0(&sprt, (x0 + x1) / 2, (y0 + y1) / 2);
    setUV0(&sprt, 0, 0);
    setWH(&sprt, 4, 4);
    sprt.clut = getClut(16, 480);
    catPrim(&black, &sprt);
    termPrim(&sprt);
    DrawOTag((u_long *)&black);
}

static void draw_black_quad(int x0, int y0, int x1, int y1) {
    static POLY_F4 black;

    memset(&black, 0, sizeof(black));
    setPolyF4(&black);
    setRGB0(&black, 0, 0, 0);
    setXY4(&black, x0, y0, x1, y0, x0, y1, x1, y1);
    termPrim(&black);
    DrawOTag((u_long *)&black);
}

static void draw_tex_sprite(int x, int y) {
    static SPRT sprt;
    static DR_TPAGE tpage;

    memset(&tpage, 0, sizeof(tpage));
    setDrawTPage(&tpage, 0, 0, getTPage(2, 0, 640, 256));
    termPrim(&tpage);
    DrawOTag((u_long *)&tpage);
    memset(&sprt, 0, sizeof(sprt));
    setSprt(&sprt);
    setRGB0(&sprt, 128, 128, 128);
    setXY0(&sprt, x, y);
    setUV0(&sprt, 0, 0);
    setWH(&sprt, 4, 4);
    sprt.clut = getClut(16, 480);
    termPrim(&sprt);
    DrawOTag((u_long *)&sprt);
}

static void draw_screen_overlay(void) {
    static POLY_F4 overlay;

    memset(&overlay, 0, sizeof(overlay));
    setPolyF4(&overlay);
    setRGB0(&overlay, 207, 207, 207);
    setXY4(&overlay, 0, 0, 320, 0, 0, 240, 320, 240);
    termPrim(&overlay);
    DrawOTag((u_long *)&overlay);
}

int tmd_vram_self_check(void) {
    static uint16_t zeros[16 * 4];
    RECT rect;
    RECT elsewhere;
    int failed;
    const char *sample;

    debug_begin();
    setRECT(&rect, 100, 200, 8, 2);
    memset(zeros, 0, sizeof(zeros));
    LoadImage(&rect, (u_long *)zeros);
    setRECT(&elsewhere, 400, 100, 4, 4);
    MoveImage(&elsewhere, 420, 110);
    ClearImage(&elsewhere, 0, 0, 0);
    log_cover(104, 201);
    log_cover(0, 0);
    failed = check(strstr(debug_log(), "tmdvram: cover 104,201 rect load x 100 y 200 w 8 h 2") != NULL,
                   "a LoadImage rectangle covers a known address");
    failed |= check(strstr(debug_log(), "tmdvram: cover 0,0 rect none") != NULL,
                    "an address no rectangle covers logs rect none");

    debug_begin();
    while (rect_n < RECT_CAP_MAX) {
        tmd_vram_note_rect(RECT_LOAD, 1, 1, 1, 1);
    }
    tmd_vram_note_rect(RECT_LOAD, 100, 200, 8, 2);
    log_cover(104, 201);
    failed |= check(strstr(debug_log(), "tmdvram: cover 104,201 rect load x 100 y 200 w 8 h 2") != NULL,
                    "a full history keeps the newest rectangle");

    debug_begin();
    setRECT(&rect, 640, 256, 16, 4);
    LoadImage(&rect, (u_long *)zeros);
    host_clear();
    draw_sprite_over(10, 10, 40, 40);
    failed |= check(capture() == 1, "a constructed ordering table dumps one frame");
    sample = debug_log();
    failed |= check(strstr(sample, "tmdvram: sample cmd 0x64 page 640,256 uv 0,0 clut 16,480 raw 0x0000 index none color 0x0000") !=
                            NULL &&
                        strstr(sample, "rect load x 640 y 256 w 16 h 4") != NULL,
                    "the primitive logs command, page, uv, clut, raw word, index, color, and rectangle");

    debug_begin();
    setRECT(&rect, 640, 256, 16, 4);
    LoadImage(&rect, (u_long *)zeros);
    host_clear();
    draw_screen_overlay();
    draw_sprite_over(10, 10, 40, 40);
    failed |= check(capture() == 1 && strstr(debug_log(), "tmdvram: skip fade") == NULL &&
                        strstr(debug_log(), "tmdvram: sample cmd 0x64 page 640,256 uv 0,0 clut 16,480 raw 0x0000") != NULL,
                    "a full-screen untextured overlay still dumps the textured primitive under it");

    debug_begin();
    setRECT(&rect, 640, 256, 16, 4);
    LoadImage(&rect, (u_long *)zeros);
    host_clear();
    draw_black_quad(10, 10, 40, 40);
    draw_tex_sprite(200, 180);
    failed |= check(capture() == 1 && strstr(debug_log(), "tmdvram: skip blob clear") == NULL &&
                        strstr(debug_log(), "tmdvram: blob clear ignored") != NULL &&
                        strstr(debug_log(), "tmdvram: sample cmd 0x64 page 640,256 uv 0,0 clut 16,480 raw 0x0000") != NULL,
                    "an opaque fill does not drop a textured primitive that samples 0x0000");

    debug_begin();
    {
        static uint16_t painted[1];

        painted[0] = 0x7FFF;
        setRECT(&rect, 640, 256, 1, 1);
        LoadImage(&rect, (u_long *)painted);
    }
    host_clear();
    draw_black_quad(10, 10, 40, 40);
    draw_tex_sprite(200, 180);
    failed |= check(capture() == 0 && strstr(debug_log(), "tmdvram: skip blob clear") != NULL &&
                        strstr(debug_log(), "tmdvram: sample") == NULL,
                    "a blob with no stored-zero primitive still writes no dump");
    setRECT(&rect, 640, 256, 16, 4);
    LoadImage(&rect, (u_long *)zeros);
    debug_on = 0;

    log_len = 0;
    if (log_text != NULL) {
        log_text[0] = '\0';
    }
    dump_n = 0;
    dumped = 0;
    f8_was = 0;
    force_enabled = 1;
    host_clear();
    draw_sprite_over(0, 0, 200, 240);
    tmd_vram_present(1);
    failed |= check(strstr(debug_log(), "tmdvram: dump") == NULL && dumped == 0,
                    "a full-height blob with nothing at the center writes no dump and leaves the arm set");
    tmd_vram_present(0);
    host_clear();
    draw_sprite_over(80, 0, 240, 240);
    tmd_vram_present(1);
    failed |= check(strstr(debug_log(), "tmdvram: dump 1") != NULL &&
                        strstr(debug_log(), "tmdvram: blob full-height x0 80 y0 0 x1 240 y1 240 center 160,120") != NULL &&
                        strstr(debug_log(), "tmdvram: sample cmd 0x64 page 640,256 uv 0,0 clut 16,480") != NULL,
                    "a full-height blob dumps the textured primitive at the center");

    force_enabled = -1;
    debug_on = 0;
    dumped = 0;
    f8_was = 0;
    dump_n = 0;
    rect_n = 0;
    rect_head = 0;
    farm_was = 0;
    host_clear();
    return failed;
}
