/* Writes one armed frame of textured primitives as BMPs the mods loader can be given.
 * The pixels are the colors already stored in VRAM. Nothing here writes VRAM back. */

#include <texture_dump.h>

#include <host.h>
#include <libgpu.h>
#include <mods.h>
#include <vram.h>

#include <SDL.h>

#include <direct.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LOG_MAX 16384

typedef struct Note {
    int page_x;
    int page_y;
    int depth;
    int clut_x;
    int clut_y;
    int u0;
    int v0;
    int u1;
    int v1;
} Note;

static Note *notes;
static int note_n;
static int note_cap;
static int f8_was;
static int dump_n;
static char *log_text;
static int log_len;

static int armed(void) {
    const char *env;

    env = getenv("HMBTN_DUMP_TEXTURES");
    return env != NULL && strcmp(env, "1") == 0;
}

static void add_line(const char *fmt, ...) {
    char buf[384];
    va_list ap;
    int n;

    va_start(ap, fmt);
    n = vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    if (n < 0) {
        return;
    }
    SDL_Log("%s", buf);
    if (log_text == NULL) {
        log_text = (char *)malloc(LOG_MAX);
        if (log_text != NULL) {
            log_text[0] = '\0';
            log_len = 0;
        }
    }
    if (log_text == NULL || log_len + n + 2 >= LOG_MAX) {
        return;
    }
    memcpy(log_text + log_len, buf, (size_t)n);
    log_len += n;
    log_text[log_len++] = '\n';
    log_text[log_len] = '\0';
}

static void clear_notes(void) {
    free(notes);
    notes = NULL;
    note_n = 0;
    note_cap = 0;
}

static void order_pair(int *lo, int *hi) {
    int tmp;

    if (*hi < *lo) {
        tmp = *lo;
        *lo = *hi;
        *hi = tmp;
    }
}

static int same_key(const Note *note, int page_x, int page_y, int u0, int v0, int u1, int v1) {
    return note->page_x == page_x && note->page_y == page_y && note->u0 == u0 && note->v0 == v0 && note->u1 == u1 &&
           note->v1 == v1;
}

void texture_dump_note(int page_x, int page_y, int depth, int clut_x, int clut_y, int u0, int v0, int u1, int v1) {
    Note *grown;
    int cap;
    int i;

    if (!armed()) {
        return;
    }
    order_pair(&u0, &u1);
    order_pair(&v0, &v1);
    for (i = 0; i < note_n; i++) {
        if (same_key(&notes[i], page_x, page_y, u0, v0, u1, v1)) {
            add_line("dumptex: duplicate %d_%d_%d_%d_%d_%d.bmp", page_x, page_y, u0, v0, u1, v1);
            return;
        }
    }
    if (note_n == note_cap) {
        cap = note_cap == 0 ? 32 : note_cap * 2;
        grown = (Note *)realloc(notes, (size_t)cap * sizeof(Note));
        if (grown == NULL) {
            add_line("dumptex: notes full");
            return;
        }
        notes = grown;
        note_cap = cap;
    }
    notes[note_n].page_x = page_x;
    notes[note_n].page_y = page_y;
    notes[note_n].depth = depth;
    notes[note_n].clut_x = clut_x;
    notes[note_n].clut_y = clut_y;
    notes[note_n].u0 = u0;
    notes[note_n].v0 = v0;
    notes[note_n].u1 = u1;
    notes[note_n].v1 = v1;
    note_n++;
}

static unsigned sample_color(const Note *note, int u, int v) {
    int x;
    int y;
    unsigned word;
    int index;

    if (note->depth <= 4) {
        x = note->page_x + u / 4;
        y = note->page_y + v;
        word = vram[y & 511][x & 1023];
        index = (int)((word >> ((u & 3) * 4)) & 0xFu);
        return vram[note->clut_y & 511][(note->clut_x + index) & 1023];
    }
    if (note->depth <= 8) {
        x = note->page_x + u / 2;
        y = note->page_y + v;
        word = vram[y & 511][x & 1023];
        index = (u & 1) ? (int)((word >> 8) & 0xFFu) : (int)(word & 0xFFu);
        return vram[note->clut_y & 511][(note->clut_x + index) & 1023];
    }
    return vram[(note->page_y + v) & 511][(note->page_x + u) & 1023];
}

static void store_pixel(SDL_Surface *image, int x, int y, unsigned color) {
    Uint8 *row;
    Uint32 mapped;
    unsigned r;
    unsigned g;
    unsigned b;
    int bpp;

    r = (color & 31u) * 255u / 31u;
    g = ((color >> 5) & 31u) * 255u / 31u;
    b = ((color >> 10) & 31u) * 255u / 31u;
    mapped = SDL_MapRGB(image->format, (Uint8)r, (Uint8)g, (Uint8)b);
    bpp = image->format->BytesPerPixel;
    row = (Uint8 *)image->pixels + (size_t)y * (size_t)image->pitch + (size_t)x * (size_t)bpp;
    memcpy(row, &mapped, (size_t)bpp);
}

static int dump_root(char *out, int cap) {
    char *base;
    int n;

    base = SDL_GetBasePath();
    if (base == NULL) {
        return -1;
    }
    n = snprintf(out, (size_t)cap, "%smods/textures/dump", base);
    SDL_free(base);
    if (n < 0 || n >= cap) {
        return -1;
    }
    return 0;
}

static int ensure_dump_dir(void) {
    char *base;
    char path[512];
    int n;

    base = SDL_GetBasePath();
    if (base == NULL) {
        return -1;
    }
    n = snprintf(path, sizeof path, "%smods", base);
    if (n > 0 && n < (int)sizeof path) {
        _mkdir(path);
    }
    n = snprintf(path, sizeof path, "%smods/textures", base);
    if (n > 0 && n < (int)sizeof path) {
        _mkdir(path);
    }
    n = snprintf(path, sizeof path, "%smods/textures/dump", base);
    SDL_free(base);
    if (n < 0 || n >= (int)sizeof path) {
        return -1;
    }
    _mkdir(path);
    return 0;
}

static int write_note(const Note *note, const char *dir) {
    SDL_Surface *image;
    char name[64];
    char path[640];
    int w;
    int h;
    int x;
    int y;
    int n;

    w = note->u1 - note->u0 + 1;
    h = note->v1 - note->v0 + 1;
    if (w <= 0 || h <= 0 || w > 256 || h > 256) {
        add_line("dumptex: skip page %d,%d uv %d,%d,%d,%d", note->page_x, note->page_y, note->u0, note->v0, note->u1,
                 note->v1);
        return 0;
    }
    n = snprintf(name, sizeof name, "%d_%d_%d_%d_%d_%d.bmp", note->page_x, note->page_y, note->u0, note->v0, note->u1,
                 note->v1);
    if (n < 0 || n >= (int)sizeof name) {
        return 0;
    }
    n = snprintf(path, sizeof path, "%s/%s", dir, name);
    if (n < 0 || n >= (int)sizeof path) {
        return 0;
    }
    image = SDL_CreateRGBSurface(0, w, h, 24, 0x000000FFu, 0x0000FF00u, 0x00FF0000u, 0);
    if (image == NULL) {
        add_line("dumptex: surface %s", name);
        return 0;
    }
    if (SDL_MUSTLOCK(image)) {
        SDL_LockSurface(image);
    }
    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            store_pixel(image, x, y, sample_color(note, note->u0 + x, note->v0 + y));
        }
    }
    if (SDL_MUSTLOCK(image)) {
        SDL_UnlockSurface(image);
    }
    if (SDL_SaveBMP(image, path) != 0) {
        SDL_FreeSurface(image);
        add_line("dumptex: save %s", name);
        return 0;
    }
    SDL_FreeSurface(image);
    add_line("dumptex: file %s", name);
    return 1;
}

static int write_notes(void) {
    char dir[512];
    int files;
    int i;

    files = 0;
    if (ensure_dump_dir() != 0 || dump_root(dir, (int)sizeof dir) != 0) {
        add_line("dumptex: no dump folder");
    } else {
        for (i = 0; i < note_n; i++) {
            files += write_note(&notes[i], dir);
        }
    }
    dump_n++;
    add_line("dumptex: dump %d files %d", dump_n, files);
    return files;
}

void texture_dump_present(int f8_down) {
    int rising;

    if (!armed()) {
        f8_was = f8_down ? 1 : 0;
        clear_notes();
        return;
    }
    rising = f8_down && !f8_was;
    f8_was = f8_down ? 1 : 0;
    if (!rising) {
        clear_notes();
        return;
    }
    write_notes();
    clear_notes();
}

static int check(int ok, const char *what) {
    SDL_Log("dumptex check %s: %s", ok ? "ok" : "FAILED", what);
    return ok ? 0 : 1;
}

static const char *debug_log(void) {
    return log_text != NULL ? log_text : "";
}

static int file_exists(const char *path) {
    FILE *file;

    file = fopen(path, "rb");
    if (file == NULL) {
        return 0;
    }
    fclose(file);
    return 1;
}

static int read_pixel(const char *path, int x, int y, int *r, int *g, int *b) {
    SDL_Surface *image;
    Uint8 *row;
    Uint32 pix;
    Uint8 rr;
    Uint8 gg;
    Uint8 bb;
    int bpp;

    image = SDL_LoadBMP(path);
    if (image == NULL || x < 0 || y < 0 || x >= image->w || y >= image->h) {
        if (image != NULL) {
            SDL_FreeSurface(image);
        }
        return -1;
    }
    if (SDL_MUSTLOCK(image)) {
        SDL_LockSurface(image);
    }
    bpp = image->format->BytesPerPixel;
    row = (Uint8 *)image->pixels + (size_t)y * (size_t)image->pitch + (size_t)x * (size_t)bpp;
    pix = 0;
    memcpy(&pix, row, (size_t)bpp);
    SDL_GetRGB(pix, image->format, &rr, &gg, &bb);
    if (SDL_MUSTLOCK(image)) {
        SDL_UnlockSurface(image);
    }
    SDL_FreeSurface(image);
    *r = (int)rr;
    *g = (int)gg;
    *b = (int)bb;
    return 0;
}

static void draw_pair(int twice) {
    static POLY_FT4 first;
    static POLY_FT4 second;
    int tp;

    tp = getTPage(0, 0, 960, 256);
    memset(&first, 0, sizeof first);
    setPolyFT4(&first);
    setShadeTex(&first, 1);
    setXY4(&first, 10, 10, 12, 10, 10, 12, 12, 12);
    setUV4(&first, 200, 201, 201, 201, 200, 201, 201, 201);
    first.clut = (u_short)getClut(32, 510);
    first.tpage = (u_short)tp;
    if (!twice) {
        termPrim(&first);
        DrawOTag((u_long *)&first);
        return;
    }
    memset(&second, 0, sizeof second);
    second = first;
    catPrim(&first, &second);
    termPrim(&second);
    DrawOTag((u_long *)&first);
}

static void plant_texels(void) {
    uint16_t word;
    uint16_t clut[2];

    word = 0x0010;
    clut[0] = 0x0000;
    clut[1] = 0x001F;
    vram_load(1010, 457, 1, 1, &word);
    vram_load(32, 510, 2, 1, clut);
}

int texture_dump_self_check(void) {
    static char env_on[] = "HMBTN_DUMP_TEXTURES=1";
    static char env_off[] = "HMBTN_DUMP_TEXTURES=0";
    static char env_clear[] = "HMBTN_DUMP_TEXTURES=";
    uint16_t saved_word;
    uint16_t saved_clut0;
    uint16_t saved_clut1;
    char dir[512];
    char path[640];
    unsigned char before[3];
    unsigned char after[3];
    int r;
    int g;
    int b;
    int failed;
    unsigned name;

    failed = 0;
    r = g = b = -1;
    saved_word = vram[457][1010];
    saved_clut0 = vram[510][32];
    saved_clut1 = vram[510][33];
    plant_texels();
    _putenv(env_on);
    log_len = 0;
    if (log_text != NULL) {
        log_text[0] = '\0';
    }
    dump_n = 0;
    f8_was = 0;
    texture_dump_present(0);
    host_clear();
    draw_pair(1);
    gpu_debug_read_pixel(11, 11, before);
    texture_dump_present(1);
    failed |= check(vram[457][1010] == 0x0010 && vram[510][32] == 0x0000 && vram[510][33] == 0x001F,
                    "the dump leaves the stored words unchanged");
    gpu_debug_read_pixel(11, 11, after);
    failed |= check(before[0] == after[0] && before[1] == after[1] && before[2] == after[2],
                    "the dump leaves the frame pixel unchanged");
    failed |= check(strstr(debug_log(), "dumptex: duplicate 960_256_200_201_201_201.bmp") != NULL &&
                        strstr(debug_log(), "dumptex: file 960_256_200_201_201_201.bmp") != NULL &&
                        strstr(debug_log(), "dumptex: dump 1 files 1") != NULL,
                    "one rising F8 writes one file for two primitives that share a page and UV");
    if (dump_root(dir, (int)sizeof dir) != 0) {
        failed |= check(0, "the dump folder is beside the executable");
    } else {
        snprintf(path, sizeof path, "%s/960_256_200_201_201_201.bmp", dir);
        failed |= check(read_pixel(path, 0, 0, &r, &g, &b) == 0 && r == 0 && g == 0 && b == 0,
                        "a stored 0x0000 stays black");
        SDL_Log("dumptex check zero %d,%d,%d", r, g, b);
        failed |= check(read_pixel(path, 1, 0, &r, &g, &b) == 0 && r == 255 && g == 0 && b == 0,
                        "the clut color is the bmp pixel");
        SDL_Log("dumptex check color %d,%d,%d", r, g, b);
        mods_rescan();
        name = (unsigned)texture_resolve(960, 256, 4, 32, 510, 200, 201, 201, 201, NULL);
        failed |= check(name == 0, "a file only under dump/ stays on vram");
        SDL_Log("dumptex check resolve 0x%x", name);
        remove(path);
        host_clear();
        draw_pair(0);
        texture_dump_present(1);
        failed |= check(!file_exists(path) && strstr(debug_log(), "dumptex: dump 2") == NULL,
                        "holding F8 writes nothing on the next frame");
        _putenv(env_off);
        texture_dump_present(0);
        host_clear();
        draw_pair(0);
        texture_dump_present(1);
        failed |= check(!file_exists(path) && strstr(debug_log(), "dumptex: dump 2") == NULL,
                        "an unarmed F8 writes nothing");
    }
    vram_load(1010, 457, 1, 1, &saved_word);
    vram_load(32, 510, 1, 1, &saved_clut0);
    vram_load(33, 510, 1, 1, &saved_clut1);
    _putenv(env_clear);
    texture_dump_present(0);
    clear_notes();
    return failed;
}
