/* Texture-mod checks, run at startup when HMBTN_SELFTEST=1. */

#include <host.h>
#include <libgpu.h>
#include <mods.h>
#include <vram.h>

#include <SDL.h>

#include <direct.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char env_mods[640];

static int check(int ok, const char *what) {
    SDL_Log("mods check %s: %s", ok ? "ok" : "FAILED", what);
    return ok ? 0 : 1;
}

static void write_red(const char *path) {
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
        pix[i * 3 + 2] = 255;
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

static void join(char *out, int cap, const char *dir, const char *name) {
    snprintf(out, (size_t)cap, "%s/%s", dir, name);
}

static POLY_FT4 mod_quad;

static int draw_overlay(void) {
    uint16_t marker;
    uint16_t blue;
    unsigned char rgb[3];
    int tp;
    int failed;

    failed = 0;
    marker = 0x1234;
    blue = 0x7C00;
    vram_load(896, 256, 1, 1, &marker);
    vram_load(912, 256, 1, 1, &blue);
    tp = getTPage(2, 0, 896, 256);

    host_clear();
    memset(&mod_quad, 0, sizeof mod_quad);
    setPolyFT4(&mod_quad);
    setShadeTex(&mod_quad, 1);
    setXY4(&mod_quad, 40, 30, 47, 30, 40, 37, 47, 37);
    setUV4(&mod_quad, 0, 0, 7, 0, 0, 7, 7, 7);
    mod_quad.tpage = (u_short)tp;
    termPrim(&mod_quad);
    DrawOTag((u_long *)&mod_quad);
    gpu_debug_read_pixel(43, 33, rgb);
    failed |= check(rgb[0] > 200 && rgb[1] < 40 && rgb[2] < 40, "the bmp primitive is red");
    SDL_Log("mods check bmp pixel %d,%d,%d", rgb[0], rgb[1], rgb[2]);
    failed |= check(vram[256][896] == marker, "the bmp primitive leaves vram unchanged");

    host_clear();
    memset(&mod_quad, 0, sizeof mod_quad);
    setPolyFT4(&mod_quad);
    setShadeTex(&mod_quad, 1);
    setXY4(&mod_quad, 60, 30, 67, 30, 60, 37, 67, 37);
    setUV4(&mod_quad, 16, 0, 16, 0, 16, 0, 16, 0);
    mod_quad.tpage = (u_short)tp;
    termPrim(&mod_quad);
    DrawOTag((u_long *)&mod_quad);
    gpu_debug_read_pixel(63, 33, rgb);
    failed |= check(rgb[2] > 200 && rgb[0] < 40, "another uv samples vram");
    SDL_Log("mods check vram pixel %d,%d,%d", rgb[0], rgb[1], rgb[2]);
    host_clear();
    return failed;
}

int mods_self_check(void) {
    char *base;
    char dir[512];
    char path[640];
    unsigned name;
    int failed;

    failed = 0;
    base = SDL_GetBasePath();
    if (base == NULL) {
        SDL_Log("mods check FAILED: no executable folder");
        return 1;
    }
    snprintf(dir, sizeof dir, "%smods-selftest", base);
    SDL_free(base);
    _mkdir(dir);

    join(path, (int)sizeof path, dir, "3_5_0_0_7_7.bmp");
    write_red(path);
    join(path, (int)sizeof path, dir, "896_256_0_0_7_7.bmp");
    write_red(path);
    join(path, (int)sizeof path, dir, "a_4_4_0_0_2_2.bmp");
    write_red(path);
    join(path, (int)sizeof path, dir, "b_4_4_0_0_2_2.bmp");
    write_red(path);
    join(path, (int)sizeof path, dir, "9_9_0_0_1_1.bmp");
    write_junk(path);

    snprintf(env_mods, sizeof env_mods, "HMBTN_MODS=%s", dir);
    _putenv(env_mods);
    mods_rescan();

    name = (unsigned)texture_resolve(3, 5, 15, 0, 0, 0, 0, 7, 7, NULL);
    failed |= check(name != 0, "one bmp returns its name");
    SDL_Log("mods check name 0x%x", name);
    failed |= check((unsigned)texture_resolve(3, 5, 15, 0, 0, 1, 0, 7, 7, NULL) == 0, "another uv stays on vram");
    failed |= check((unsigned)texture_resolve(4, 4, 15, 0, 0, 0, 0, 2, 2, NULL) == 0, "two files stay on vram");
    failed |= check((unsigned)texture_resolve(9, 9, 15, 0, 0, 0, 0, 1, 1, NULL) == 0, "a broken bmp stays on vram");
    failed |= draw_overlay();

    _putenv("HMBTN_MODS=");
    mods_rescan();
    join(path, (int)sizeof path, dir, "3_5_0_0_7_7.bmp");
    remove(path);
    join(path, (int)sizeof path, dir, "896_256_0_0_7_7.bmp");
    remove(path);
    join(path, (int)sizeof path, dir, "a_4_4_0_0_2_2.bmp");
    remove(path);
    join(path, (int)sizeof path, dir, "b_4_4_0_0_2_2.bmp");
    remove(path);
    join(path, (int)sizeof path, dir, "9_9_0_0_1_1.bmp");
    remove(path);
    _rmdir(dir);
    return failed;
}
