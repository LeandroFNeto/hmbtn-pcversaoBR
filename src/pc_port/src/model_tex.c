/* Stamps the TMD header onto the primitive bytes built while a model draws.
 * The game global current_tmd is only read by the self-check. This file never writes it. */

#include <model_tex.h>

#include <frame_buffers.h>
#include <host.h>
#include <libgpu.h>
#include <main.h>
#include <model.h>
#include <mods.h>
#include <vram.h>

#include <SDL.h>

#include <direct.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

extern TmdHeader *current_tmd;
extern FrameState *current_frame;

void __real_model_draw(Model *model);
s16 __real_model_draw_visible(Model *obj, VECTOR *eye, VECTOR *target);
void __real_debug_hook_run(s32 code);

#define RANGE_MAX 128
#define SEEN_MAX 64
#define LOG_MAX 4096

typedef struct Range {
    const unsigned char *start;
    const unsigned char *end;
    const void *tmd;
} Range;

static Range ranges[RANGE_MAX];
static int range_n;
static const void *host_tmd;
static const unsigned char *open_start;
static const void *seen[SEEN_MAX];
static int seen_n;
static char log_text[LOG_MAX];
static int log_len;

static void add_line(const char *fmt, ...) {
    char buf[96];
    va_list ap;
    int n;

    va_start(ap, fmt);
    n = vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    if (n < 0) {
        return;
    }
    SDL_Log("%s", buf);
    if (log_len + n + 2 >= LOG_MAX) {
        return;
    }
    memcpy(log_text + log_len, buf, (size_t)n);
    log_len += n;
    log_text[log_len++] = '\n';
    log_text[log_len] = '\0';
}

static void log_once(const void *tmd) {
    int i;

    if (tmd == NULL) {
        return;
    }
    for (i = 0; i < seen_n; i++) {
        if (seen[i] == tmd) {
            return;
        }
    }
    if (seen_n < SEEN_MAX) {
        seen[seen_n++] = tmd;
    }
    add_line("modeltex: tmd_%08x", (unsigned)(uintptr_t)tmd);
}

static void close_open(const unsigned char *cursor) {
    if (host_tmd == NULL || open_start == NULL || cursor == NULL || cursor <= open_start) {
        return;
    }
    if (range_n >= RANGE_MAX) {
        return;
    }
    ranges[range_n].start = open_start;
    ranges[range_n].end = cursor;
    ranges[range_n].tmd = host_tmd;
    range_n++;
    log_once(host_tmd);
}

void model_tex_drop_below(const void *cursor) {
    const unsigned char *at;
    int i;
    int kept;

    at = (const unsigned char *)cursor;
    if (at == NULL) {
        return;
    }
    kept = 0;
    for (i = 0; i < range_n; i++) {
        if (ranges[i].start > at) {
            continue;
        }
        ranges[kept++] = ranges[i];
    }
    range_n = kept;
    if (open_start != NULL && open_start > at) {
        open_start = NULL;
    }
}

void model_tex_begin(const void *tmd) {
    host_tmd = tmd;
    open_start = NULL;
}

void model_tex_end(void) {
    if (current_frame != NULL) {
        close_open(current_frame->primCursor.bytes);
        model_tex_drop_below(current_frame->primCursor.bytes);
    }
    host_tmd = NULL;
    open_start = NULL;
}

void model_tex_hook(int code) {
    const unsigned char *cursor;

    if (code != 0x2000 || host_tmd == NULL || current_frame == NULL) {
        return;
    }
    cursor = current_frame->primCursor.bytes;
    if (cursor == NULL) {
        return;
    }
    model_tex_drop_below(cursor);
    if (open_start != NULL && cursor > open_start) {
        close_open(cursor);
    }
    open_start = cursor;
}

void model_tex_finish(void) {
    range_n = 0;
    open_start = NULL;
}

const void *model_tex_lookup(const void *packet) {
    const unsigned char *at;
    const void *found;
    int i;

    at = (const unsigned char *)packet;
    found = NULL;
    if (at == NULL) {
        return NULL;
    }
    for (i = 0; i < range_n; i++) {
        if (at >= ranges[i].start && at < ranges[i].end) {
            found = ranges[i].tmd;
        }
    }
    return found;
}

void __wrap_model_draw(Model *model) {
    model_tex_begin(model != NULL ? model->tmd : NULL);
    __real_model_draw(model);
    model_tex_end();
}

s16 __wrap_model_draw_visible(Model *obj, VECTOR *eye, VECTOR *target) {
    s16 drawn;

    model_tex_begin(obj != NULL ? obj->tmd : NULL);
    drawn = __real_model_draw_visible(obj, eye, target);
    model_tex_end();
    return drawn;
}

void __wrap_debug_hook_run(s32 code) {
    model_tex_hook((int)code);
    __real_debug_hook_run(code);
}

static int check(int ok, const char *what) {
    SDL_Log("modeltex check %s: %s", ok ? "ok" : "FAILED", what);
    return ok ? 0 : 1;
}

static int count_line(const char *line) {
    const char *p;
    int n;

    n = 0;
    p = log_text;
    while (p != NULL && *p != '\0') {
        p = strstr(p, line);
        if (p == NULL) {
            break;
        }
        n++;
        p += strlen(line);
    }
    return n;
}

static void cover(unsigned char *start, int bytes, const void *tmd) {
    FrameState *saved;
    static FrameState frame;

    saved = current_frame;
    memset(&frame, 0, sizeof frame);
    current_frame = &frame;
    frame.primCursor.bytes = start;
    model_tex_begin(tmd);
    model_tex_hook(0x2000);
    frame.primCursor.bytes = start + bytes;
    model_tex_end();
    current_frame = saved;
}

static void write_bmp(const char *path, int r, int g, int b) {
    unsigned char hdr[54];
    unsigned char pix[8 * 8 * 3];
    FILE *file;
    int i;

    memset(hdr, 0, sizeof hdr);
    hdr[0] = 'B';
    hdr[1] = 'M';
    hdr[2] = (unsigned char)(54 + sizeof pix);
    hdr[10] = 54;
    hdr[14] = 40;
    hdr[18] = 8;
    hdr[22] = 8;
    hdr[26] = 1;
    hdr[28] = 24;
    for (i = 0; i < 8 * 8; i++) {
        pix[i * 3] = (unsigned char)b;
        pix[i * 3 + 1] = (unsigned char)g;
        pix[i * 3 + 2] = (unsigned char)r;
    }
    file = fopen(path, "wb");
    if (file == NULL) {
        return;
    }
    fwrite(hdr, 1, sizeof hdr, file);
    fwrite(pix, 1, sizeof pix, file);
    fclose(file);
}

static void write_junk(const char *path) {
    FILE *file;

    file = fopen(path, "wb");
    if (file == NULL) {
        return;
    }
    fputs("not a bmp", file);
    fclose(file);
}

static POLY_FT4 quad_a;
static POLY_FT4 quad_b;
static POLY_FT4 quad_none;
static POLY_FT4 quad_vram;
static POLY_FT4 quad_dup;
static POLY_FT4 quad_bad;

static void prime_quad(POLY_FT4 *quad, int x, int u, int tp) {
    memset(quad, 0, sizeof *quad);
    setPolyFT4(quad);
    setShadeTex(quad, 1);
    setXY4(quad, x, 30, x + 7, 30, x, 37, x + 7, 37);
    setUV4(quad, u, 0, u + 7, 0, u, 7, u + 7, 7);
    quad->tpage = (u_short)tp;
    termPrim(quad);
}

static int pixel_is(int x, int r, int g, int b) {
    unsigned char rgb[3];

    gpu_debug_read_pixel(x, 33, rgb);
    return rgb[0] > r - 40 && rgb[0] < r + 40 && rgb[1] > g - 40 && rgb[1] < g + 40 && rgb[2] > b - 40 &&
           rgb[2] < b + 40;
}

int model_tex_self_check(void) {
    unsigned char mem[64];
    TmdHeader *saved_tmd;
    FrameState *saved_frame;
    char *base;
    char dir[512];
    char path[640];
    char env_mods[640];
    char line_a[40];
    const void *a;
    const void *b;
    uint16_t marker;
    uint16_t blue;
    int tp;
    int failed;

    failed = 0;
    a = (const void *)0xA;
    b = (const void *)0xB;
    range_n = 0;
    open_start = NULL;
    host_tmd = NULL;
    seen_n = 0;
    log_len = 0;
    log_text[0] = '\0';

    cover(mem + 8, 24, a);
    failed |= check(model_tex_lookup(mem + 8) == a && model_tex_lookup(mem + 31) == a,
                    "an address inside the range returns that pointer");
    failed |= check(model_tex_lookup(mem + 32) == NULL && model_tex_lookup(mem) == NULL,
                    "an address outside the range returns none");
    model_tex_drop_below(mem);
    failed |= check(model_tex_lookup(mem + 8) == NULL, "a cursor below the range drops it");

    saved_tmd = current_tmd;
    saved_frame = current_frame;
    current_frame = NULL;
    model_tex_begin(a);
    __wrap_debug_hook_run(0x2000);
    model_tex_end();
    __wrap_debug_hook_run(0);
    failed |= check(current_tmd == saved_tmd, "the game tmd global is not written");
    failed |= check(host_tmd == NULL, "a null frame leaves the host pointer clear");
    current_frame = saved_frame;

    log_len = 0;
    log_text[0] = '\0';
    seen_n = 0;
    cover(mem, 16, a);
    cover(mem + 16, 16, a);
    snprintf(line_a, sizeof line_a, "modeltex: tmd_%08x", (unsigned)(uintptr_t)a);
    failed |= check(count_line(line_a) == 1, "the first use of an address logs once");
    model_tex_finish();

    base = SDL_GetBasePath();
    if (base == NULL) {
        return check(0, "the mods folder is beside the executable");
    }
    snprintf(dir, sizeof dir, "%smods-model-selftest", base);
    SDL_free(base);
    _mkdir(dir);
    snprintf(path, sizeof path, "%s/tmd_%08x.bmp", dir, (unsigned)(uintptr_t)a);
    write_bmp(path, 0, 255, 0);
    snprintf(path, sizeof path, "%s/tmd_%08x.bmp", dir, (unsigned)(uintptr_t)b);
    write_bmp(path, 0, 0, 255);
    snprintf(path, sizeof path, "%s/896_256_0_0_7_7.bmp", dir);
    write_bmp(path, 255, 0, 0);
    snprintf(path, sizeof path, "%s/a_tmd_%08x.bmp", dir, 0xDu);
    write_bmp(path, 0, 255, 0);
    snprintf(path, sizeof path, "%s/b_tmd_%08x.bmp", dir, 0xDu);
    write_bmp(path, 0, 0, 255);
    snprintf(path, sizeof path, "%s/tmd_%08x.bmp", dir, 0xEu);
    write_junk(path);
    snprintf(env_mods, sizeof env_mods, "HMBTN_MODS=%s", dir);
    _putenv(env_mods);
    mods_rescan();

    tp = getTPage(2, 0, 896, 256);
    marker = 0x1234;
    blue = 0x7C00;
    vram_load(896, 256, 1, 1, &marker);
    vram_load(912, 256, 1, 1, &blue);
    prime_quad(&quad_a, 40, 0, tp);
    prime_quad(&quad_b, 80, 0, tp);
    prime_quad(&quad_none, 120, 0, tp);
    prime_quad(&quad_vram, 160, 16, tp);
    setUV4(&quad_vram, 16, 0, 16, 0, 16, 0, 16, 0);
    prime_quad(&quad_dup, 200, 0, tp);
    prime_quad(&quad_bad, 240, 0, tp);

    cover((unsigned char *)&quad_a, (int)sizeof quad_a, a);
    host_clear();
    DrawOTag((u_long *)&quad_a);
    failed |= check(pixel_is(43, 255, 0, 0) == 0 && pixel_is(43, 0, 255, 0),
                    "a model file wins over the page file");
    failed |= check(vram[256][896] == marker, "the model file leaves vram unchanged");

    cover((unsigned char *)&quad_b, (int)sizeof quad_b, b);
    host_clear();
    DrawOTag((u_long *)&quad_b);
    failed |= check(pixel_is(83, 0, 0, 255), "the second model keeps its own file");

    cover((unsigned char *)&quad_none, (int)sizeof quad_none, (const void *)0xC);
    host_clear();
    DrawOTag((u_long *)&quad_none);
    failed |= check(pixel_is(123, 255, 0, 0), "a model with no file uses the page file");

    cover((unsigned char *)&quad_vram, (int)sizeof quad_vram, (const void *)0xC);
    host_clear();
    DrawOTag((u_long *)&quad_vram);
    failed |= check(pixel_is(163, 0, 0, 255), "a model with no file samples vram");

    cover((unsigned char *)&quad_dup, (int)sizeof quad_dup, (const void *)0xD);
    host_clear();
    DrawOTag((u_long *)&quad_dup);
    failed |= check(pixel_is(203, 255, 0, 0), "two files for one address do not replace");

    cover((unsigned char *)&quad_bad, (int)sizeof quad_bad, (const void *)0xE);
    host_clear();
    DrawOTag((u_long *)&quad_bad);
    failed |= check(pixel_is(243, 255, 0, 0), "an unreadable model file does not replace");
    failed |= check(vram[256][896] == marker, "a rejected model file leaves vram unchanged");

    _putenv("HMBTN_MODS=");
    mods_rescan();
    host_clear();
    model_tex_finish();
    return failed;
}
