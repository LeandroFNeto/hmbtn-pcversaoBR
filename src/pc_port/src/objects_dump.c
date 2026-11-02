/* One frame of the commands that overlap a black blob, plus the GTE cull totals
 * and the loaded farm model's primitive headers. Allocates its note list once. */

#include <objects_dump.h>

#include <camera.h>
#include <host.h>
#include <libgpu.h>
#include <libgte.h>
#include <object_trace.h>
#include <vram.h>

#include "model.h"

#include <SDL.h>
#include <GL/gl.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int main_scene;
extern Model farm_model;
extern ClipPlane clip_planes[4];
extern VECTOR view_direction;

#define NOTE_MAX 2048
#define LOG_MAX 4096

typedef struct Note {
    unsigned cmd;
    int skipped;
    int next;
    short x[4];
    short y[4];
    int n;
    int r, g, b;
    int textured;
    int u, v;
    int page_x, page_y, clut_x, clut_y, depth;
} Note;

static Note *notes;
static int note_n;
static int note_overflow;
static int flag_neg;
static int nclip_le;
static int nclip_gt;
static int debug_on;
static int force_enabled = -1;
static int f8_was;
static int dump_n;
static char *log_text;
static int log_len;
static int *blob_queue;

static int env_enabled(void) {
    static int cached = -1;
    const char *env;

    if (cached < 0) {
        env = getenv("HMBTN_OBJECTS");
        cached = env != NULL && strcmp(env, "1") == 0;
    }
    return cached;
}

static int dump_enabled(void) {
    if (force_enabled >= 0) {
        return force_enabled;
    }
    return env_enabled();
}

int objects_dump_collecting(void) {
    return debug_on || dump_enabled();
}

static int ensure_notes(void) {
    if (notes != NULL) {
        return 1;
    }
    notes = (Note *)calloc((size_t)NOTE_MAX, sizeof(Note));
    if (blob_queue == NULL) {
        blob_queue = (int *)malloc(60 * 80 * sizeof(int));
    }
    if (log_text == NULL) {
        log_text = (char *)malloc(LOG_MAX);
        if (log_text != NULL) {
            log_text[0] = '\0';
        }
    }
    return notes != NULL && blob_queue != NULL && log_text != NULL;
}

static void add_line(const char *fmt, ...) {
    char buf[320];
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

static void reset_frame(void) {
    note_n = 0;
    note_overflow = 0;
    flag_neg = 0;
    nclip_le = 0;
    nclip_gt = 0;
}

void objects_dump_note_drawn(unsigned cmd, int n, const short *xy, int r, int g, int b, int textured, int u, int v,
                             int page_x, int page_y, int clut_x, int clut_y, int depth) {
    Note *note;
    int i;

    if (!objects_dump_collecting() || !ensure_notes()) {
        return;
    }
    if (note_n >= NOTE_MAX) {
        note_overflow = 1;
        return;
    }
    note = &notes[note_n++];
    memset(note, 0, sizeof(*note));
    note->cmd = cmd;
    note->next = -1;
    note->n = n > 4 ? 4 : n;
    note->r = r;
    note->g = g;
    note->b = b;
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

void objects_dump_note_skip(unsigned cmd, int next) {
    Note *note;

    if (!objects_dump_collecting() || !ensure_notes()) {
        return;
    }
    if (note_n >= NOTE_MAX) {
        note_overflow = 1;
        return;
    }
    note = &notes[note_n++];
    memset(note, 0, sizeof(*note));
    note->cmd = cmd;
    note->skipped = 1;
    note->next = next;
}

void objects_dump_note_flag(int value) {
    if (objects_dump_collecting() && value < 0) {
        flag_neg++;
    }
}

void objects_dump_note_nclip(int mac0) {
    if (!objects_dump_collecting()) {
        return;
    }
    if (mac0 <= 0) {
        nclip_le++;
    } else {
        nclip_gt++;
    }
}

static int on_farm(void) {
    return main_scene == 2 || main_scene == 6 || main_scene == 14 || main_scene == 15 || main_scene == 19 ||
           main_scene == 20;
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

static int is_fade_note(const Note *note) {
    int i;
    int minx;
    int miny;
    int maxx;
    int maxy;

    if (note->skipped || note->textured || note->n < 3) {
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

static unsigned sample_word(const Note *note) {
    int tu;
    int tv;
    int x;
    int y;
    unsigned word;
    int index;

    if (!note->textured) {
        return 0;
    }
    tu = note->u;
    tv = note->v;
    if (note->depth <= 4) {
        x = note->page_x + tu / 4;
        y = note->page_y + tv;
        word = vram[y & 511][x & 1023];
        index = (int)((word >> ((tu & 3) * 4)) & 0xF);
        return vram[note->clut_y & 511][(note->clut_x + index) & 1023];
    }
    if (note->depth <= 8) {
        x = note->page_x + tu / 2;
        y = note->page_y + tv;
        word = vram[y & 511][x & 1023];
        index = (tu & 1) ? (int)((word >> 8) & 0xFF) : (int)(word & 0xFF);
        return vram[note->clut_y & 511][(note->clut_x + index) & 1023];
    }
    return vram[(note->page_y + tv) & 511][(note->page_x + tu) & 1023];
}

static int prim_size(unsigned mode) {
    switch (mode & 0xFDFFu) {
    case 0x2400:
    case 0x2004:
    case 0x3101:
    case 0x3800:
        return 0x18;
    case 0x2800:
    case 0x3000:
        return 0x14;
    case 0x2501:
    case 0x3400:
    case 0x3004:
    case 0x3901:
        return 0x1C;
    case 0x3501:
    case 0x3C00:
    case 0x3804:
        return 0x24;
    case 0x2C00:
    case 0x2D01:
    case 0x2804:
        return 0x20;
    case 0x3D01:
        return 0x2C;
    case 0x4001:
        return 0xC;
    case 0x2000:
    case 0x2101:
    case 0x2901:
    case 0x5001:
        return 0x10;
    default:
        return 0;
    }
}

static int is_raw(unsigned mode) {
    switch (mode & 0xFDFFu) {
    case 0x2101:
    case 0x2501:
    case 0x2901:
    case 0x2D01:
    case 0x3101:
    case 0x3501:
    case 0x3901:
    case 0x3D01:
        return 1;
    default:
        return 0;
    }
}

static int centre_inside(const SVECTOR *pos) {
    SVECTOR delta;
    SVECTOR dir;
    int i;
    long dot;

    delta.vx = (short)((view_direction.vx - pos->vx) / 128);
    delta.vy = (short)((view_direction.vy - pos->vy) / 128);
    delta.vz = (short)((view_direction.vz - pos->vz) / 128);
    VectorNormalSS(&delta, &dir);
    for (i = 0; i < 4; i++) {
        dot = (long)clip_planes[i].x * dir.vx + (long)clip_planes[i].y * dir.vy + (long)clip_planes[i].z * dir.vz;
        if (i < 3 && dot < 0) {
            return 0;
        }
        if (i == 3 && dot < 0) {
            return 0;
        }
    }
    return 1;
}

static void log_tmd(void) {
    TmdHeader *tmd;
    TmdObject *object;
    ModelCenters *centers;
    int raw;
    int other;
    int outside;
    int i;
    int j;

    raw = 0;
    other = 0;
    outside = 0;
    tmd = farm_model.tmd;
    if (tmd == NULL || tmd->n_obj <= 0) {
        add_line("objects: tmd raw 0 other 0 frustum_out 0");
        return;
    }
    centers = farm_model.centers;
    for (i = 0; i < tmd->n_obj; i++) {
        unsigned char *prim;
        int count;

        object = &tmd->objects[i];
        prim = (unsigned char *)tmd->objects + object->primitive_top;
        count = object->n_primitive;
        for (j = 0; j < count; j++) {
            unsigned mode;
            int size;

            mode = *(unsigned short *)(prim + 2);
            size = prim_size(mode);
            if (size == 0) {
                other += count - j;
                break;
            }
            if (is_raw(mode)) {
                raw++;
            } else {
                other++;
            }
            prim += size;
        }
        if (centers != NULL) {
            if (!centre_inside((SVECTOR *)&centers->coords[i * 4])) {
                outside++;
            }
        }
    }
    if (centers == NULL) {
        add_line("objects: tmd raw %d other %d frustum_out no-centers", raw, other);
    } else {
        add_line("objects: tmd raw %d other %d frustum_out %d", raw, other, outside);
    }
}

static void log_lights(void) {
    int id;
    int vx;
    int vy;
    int vz;
    int r;
    int g;
    int b;
    int ar;
    int ag;
    int ab;
    int mode;

    for (id = 0; id < 3; id++) {
        vx = vy = vz = r = g = b = 0;
        gs_debug_flat_dir(id, &vx, &vy, &vz);
        gs_debug_flat_light(id, &r, &g, &b);
        add_line("objects: light %d dir %d,%d,%d rgb %d,%d,%d", id, vx, vy, vz, r, g, b);
    }
    ar = ag = ab = 0;
    mode = 0;
    gs_debug_ambient(&ar, &ag, &ab);
    gs_debug_light_mode(&mode);
    add_line("objects: ambient %d,%d,%d mode %d", ar, ag, ab, mode);
}

static int largest_blob(const unsigned char grid[60][80], int *x0, int *y0, int *x1, int *y1, int *cx, int *cy) {
    unsigned char seen[60][80];
    int y;
    int x;
    int best;
    int best_x0, best_y0, best_x1, best_y1, best_cx, best_cy;

    memset(seen, 0, sizeof(seen));
    best = 0;
    best_x0 = best_y0 = best_x1 = best_y1 = best_cx = best_cy = 0;
    for (y = 0; y < 60; y++) {
        for (x = 0; x < 80; x++) {
            int qh;
            int qt;
            int n;
            int sx;
            int sy;
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
            sx = 0;
            sy = 0;
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
                sx += cx0;
                sy += cy0;
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
                best_cx = (sx / n) * 4 + 2;
                best_cy = (sy / n) * 4 + 2;
            }
        }
    }
    *x0 = best_x0;
    *y0 = best_y0;
    *x1 = best_x1;
    *y1 = best_y1;
    *cx = best_cx;
    *cy = best_cy;
    return best;
}

/* 1 when a dump was written. A fade writes nothing and returns 0. */
static int capture(int require_farm) {
    unsigned char grid[60][80];
    int x0, y0, x1, y1, cx, cy;
    int cells;
    int i;

    if (require_farm && !on_farm()) {
        reset_frame();
        return 0;
    }
    if (!ensure_notes()) {
        return 0;
    }
    if (frame_is_fade()) {
        reset_frame();
        return 0;
    }
    if (!gpu_fill_black_grid(grid)) {
        reset_frame();
        return 0;
    }
    cells = largest_blob(grid, &x0, &y0, &x1, &y1, &cx, &cy);
    if (cells == 0) {
        add_line("objects: blob none");
    } else {
        add_line("objects: blob x0 %d y0 %d x1 %d y1 %d center %d,%d", x0, y0, x1, y1, cx, cy);
    }
    for (i = 0; i < note_n; i++) {
        Note *note;

        note = &notes[i];
        if (note->skipped) {
            if (note->next < 0) {
                add_line("objects: skipped cmd 0x%02x next none", note->cmd);
            } else {
                add_line("objects: skipped cmd 0x%02x next 0x%02x", note->cmd, note->next);
            }
            continue;
        }
        if (cells == 0 || !overlaps(note, x0, y0, x1, y1)) {
            continue;
        }
        if (note->textured) {
            add_line("objects: drawn cmd 0x%02x bits 0x%x overlaps blob xy %d,%d %d,%d %d,%d %d,%d rgb %d,%d,%d uv %d,%d word 0x%04x",
                     note->cmd, note->cmd & 3, note->x[0], note->y[0], note->x[1], note->y[1], note->x[2], note->y[2],
                     note->x[3], note->y[3], note->r, note->g, note->b, note->u, note->v, sample_word(note));
        } else {
            add_line("objects: drawn cmd 0x%02x bits 0x%x overlaps blob xy %d,%d %d,%d %d,%d %d,%d rgb %d,%d,%d",
                     note->cmd, note->cmd & 3, note->x[0], note->y[0], note->x[1], note->y[1], note->x[2], note->y[2],
                     note->x[3], note->y[3], note->r, note->g, note->b);
        }
    }
    if (note_overflow) {
        add_line("objects: notes overflow");
    }
    add_line("objects: gte flag_neg %d nclip_le %d nclip_gt %d", flag_neg, nclip_le, nclip_gt);
    log_tmd();
    log_lights();
    reset_frame();
    return 1;
}

void objects_dump_present(int f8_down) {
    int rising;

    if (debug_on) {
        return;
    }
    if (!dump_enabled()) {
        f8_was = f8_down ? 1 : 0;
        return;
    }
    rising = f8_down && !f8_was;
    f8_was = f8_down ? 1 : 0;
    if ((!on_farm() && force_enabled < 0) || !rising) {
        reset_frame();
        return;
    }
    if (frame_is_fade()) {
        reset_frame();
        return;
    }
    dump_n++;
    add_line("objects: dump %d", dump_n);
    capture(0);
}

void objects_dump_debug_begin(void) {
    debug_on = 1;
    ensure_notes();
    log_len = 0;
    if (log_text != NULL) {
        log_text[0] = '\0';
    }
    reset_frame();
}

int objects_dump_debug_capture(void) {
    int dumped;

    dumped = capture(0);
    debug_on = 0;
    return dumped;
}

const char *objects_dump_log(void) {
    return log_text != NULL ? log_text : "";
}

static int check(int ok, const char *what) {
    SDL_Log("objects dump check %s: %s", ok ? "ok" : "FAILED", what);
    return ok ? 0 : 1;
}

int objects_dump_self_check(void) {
    static POLY_F4 fade;
    static POLY_F4 quad;
    static struct {
        u_long tag;
        u_char r0, g0, b0, code;
        short x0, y0;
        short x1, y1;
        u_long extra;
    } line;
    int failed;

    objects_dump_debug_begin();
    host_clear();
    memset(&fade, 0, sizeof(fade));
    setPolyF4(&fade);
    setRGB0(&fade, 0, 0, 0);
    setXY4(&fade, 0, 0, 320, 0, 0, 240, 320, 240);
    termPrim(&fade);
    DrawOTag((u_long *)&fade);
    failed = check(objects_dump_debug_capture() == 0 && strstr(objects_dump_log(), "objects: drawn") == NULL,
                   "a full-screen untextured polygon is not dumped");

    objects_dump_debug_begin();
    host_clear();
    memset(&quad, 0, sizeof(quad));
    setPolyF4(&quad);
    setRGB0(&quad, 0, 0, 0);
    setXY4(&quad, 10, 10, 40, 10, 10, 40, 40, 40);
    memset(&line, 0, sizeof(line));
    setLineF2(&line);
    setlen(&line, 4);
    setXY2(&line, 0, 0, 2, 2);
    line.extra = 0x28000000u;
    catPrim(&quad, &line);
    termPrim(&line);
    DrawOTag((u_long *)&quad);
    failed |= check(objects_dump_debug_capture() == 1 && strstr(objects_dump_log(), "objects: drawn cmd 0x28") != NULL &&
                        strstr(objects_dump_log(), "overlaps blob") != NULL &&
                        strstr(objects_dump_log(), "objects: skipped cmd 0x40 next 0x28") != NULL,
                    "a polygon that overlaps the blob is drawn and a line that shares the packet is skipped, next 0x28");

    force_enabled = 1;
    f8_was = 0;
    dump_n = 0;
    log_len = 0;
    if (log_text != NULL) {
        log_text[0] = '\0';
    }
    host_clear();
    DrawOTag((u_long *)&fade);
    objects_dump_present(1);
    failed |= check(strstr(objects_dump_log(), "objects: dump") == NULL, "F8 on a fade writes nothing");
    objects_dump_present(1);
    failed |= check(strstr(objects_dump_log(), "objects: dump") == NULL, "holding F8 after a fade writes nothing");
    objects_dump_present(0);

    host_clear();
    DrawOTag((u_long *)&quad);
    objects_dump_present(1);
    failed |= check(strstr(objects_dump_log(), "objects: dump 1") != NULL, "a rising F8 dumps the frame");
    objects_dump_present(1);
    failed |= check(strstr(objects_dump_log(), "objects: dump 2") == NULL, "holding F8 does not dump the next frame");
    objects_dump_present(0);

    host_clear();
    DrawOTag((u_long *)&quad);
    objects_dump_present(1);
    failed |= check(strstr(objects_dump_log(), "objects: dump 2") != NULL, "a second rising F8 dumps another frame");
    force_enabled = -1;
    f8_was = 0;
    dump_n = 0;
    host_clear();
    return failed;
}
