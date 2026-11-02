/* Rectangles copied into VRAM since the farm scene was entered, and one frame of
 * the textured primitives that sample word 0x0000 over a black blob. */

#include <tmd_vram.h>

#include <host.h>
#include <libgpu.h>
#include <vram.h>

#include <SDL.h>

#include <stdarg.h>
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
                         int clut_x, int clut_y, int depth) {
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

static int is_fade_note(const Note *note) {
    int i;
    int minx;
    int miny;
    int maxx;
    int maxy;

    if (note->textured || note->n < 3) {
        return 0;
    }
    if (note->cmd < 0x20 || note->cmd > 0x3F || (note->cmd & 0x04) != 0) {
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
    return maxx - minx >= 320 && maxy - miny >= 240;
}

static int frame_is_fade(void) {
    int i;

    for (i = 0; i < note_n; i++) {
        if (is_fade_note(&notes[i])) {
            return 1;
        }
    }
    return 0;
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

/* 1 when a dump was written. A fade writes nothing. A full-height blob dumps
 * the center of the view instead of its outline. */
static int capture(void) {
    unsigned char grid[60][80];
    int x0, y0, x1, y1;
    int cells;
    int i;
    int use_center;
    int full_height;
    int hit;

    if (!ensure()) {
        return 0;
    }
    if (frame_is_fade()) {
        add_line("tmdvram: skip fade");
        reset_notes();
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
    for (i = 0; i < note_n; i++) {
        Note *note;
        unsigned raw;
        unsigned color;
        int index;
        char rect[96];

        note = &notes[i];
        if (!note->textured) {
            continue;
        }
        if (use_center) {
            if (!covers_point(note, 160, 120)) {
                continue;
            }
        } else if (!overlaps(note, x0, y0, x1, y1)) {
            continue;
        }
        sample_parts(note, &raw, &index, &color);
        if (color != 0) {
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
        rect_text(note, rect, (int)sizeof(rect));
        if (index < 0) {
            add_line("tmdvram: sample cmd 0x%02x page %d,%d uv %d,%d clut %d,%d raw 0x%04x index none color 0x%04x %s",
                     note->cmd, note->page_x, note->page_y, note->u, note->v, note->clut_x, note->clut_y, raw, color,
                     rect);
        } else {
            add_line("tmdvram: sample cmd 0x%02x page %d,%d uv %d,%d clut %d,%d raw 0x%04x index %d color 0x%04x %s",
                     note->cmd, note->page_x, note->page_y, note->u, note->v, note->clut_x, note->clut_y, raw, index,
                     color, rect);
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
    if (!rising || dumped) {
        reset_notes();
        return;
    }
    if (force_enabled < 0 && !on_farm()) {
        add_line("tmdvram: skip off-farm scene %d", main_scene);
        reset_notes();
        return;
    }
    if (capture()) {
        dumped = 1;
    }
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
