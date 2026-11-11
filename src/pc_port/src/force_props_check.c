/* Corner color for the shipping-bin and farm-door keys when the sample word is 0x0000. */

#include <bin_picture.h>
#include <gte.h>
#include <host.h>
#include <inline_c.h>
#include <libgpu.h>
#include <libgte.h>
#include <mods.h>
#include <vram.h>

#include "farm_state.h"
#include "main.h"
#include "model.h"

#include <SDL.h>

#include <direct.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static POLY_FT4 hole_quad;
static char env_mods[640];

static int check(int ok, const char *what) {
    SDL_Log("force props check %s: %s", ok ? "ok" : "FAILED", what);
    return ok ? 0 : 1;
}

static int near(int value, int expected) {
    int d;

    d = value - expected;
    if (d < 0) {
        d = -d;
    }
    return d <= 8;
}

static void put_word(int x, int y, uint16_t word) {
    vram_load(x, y, 1, 1, &word);
}

static void draw_key(int x, int y, int page_x, int u0, int v0, int u1, int v1, int r, int g, int b, int raw) {
    int tp;

    tp = getTPage(0, 0, page_x, 0);
    memset(&hole_quad, 0, sizeof hole_quad);
    setPolyFT4(&hole_quad);
    setSemiTrans(&hole_quad, 0);
    setShadeTex(&hole_quad, raw);
    setRGB0(&hole_quad, r, g, b);
    setXY4(&hole_quad, x, y, x + 8, y, x, y + 8, x + 8, y + 8);
    setUV4(&hole_quad, u0, v0, u1, v0, u0, v1, u1, v1);
    hole_quad.clut = getClut(0, 480);
    hole_quad.tpage = (u_short)tp;
    termPrim(&hole_quad);
    DrawOTag((u_long *)&hole_quad);
}

static void write_red(const char *path) {
    SDL_Surface *image;

    image = SDL_CreateRGBSurface(0, 8, 8, 24, 0x000000FFu, 0x0000FF00u, 0x00FF0000u, 0);
    if (image == NULL) {
        return;
    }
    SDL_FillRect(image, NULL, SDL_MapRGB(image->format, 255, 0, 0));
    SDL_SaveBMP(image, path);
    SDL_FreeSurface(image);
}

static void gte_bed_view(void) {
    gte_set_rotation_identity();
    gte_set_translation(0, 0, 0);
    SetGeomOffset(160, 120);
    SetGeomScreen(1000);
}

static int check_nclip_sign(void) {
    SVECTOR a;
    SVECTOR b;
    SVECTOR c;
    long front;
    long back;
    int failed;

    gte_bed_view();
    memset(&a, 0, sizeof a);
    memset(&b, 0, sizeof b);
    memset(&c, 0, sizeof c);
    a.vz = 1000;
    b.vx = 100;
    b.vz = 1000;
    c.vy = 100;
    c.vz = 1000;
    gte_ldv3(&a, &b, &c);
    gte_rtpt();
    gte_nclip();
    gte_stopz(&front);
    gte_ldv3(&a, &c, &b);
    gte_rtpt();
    gte_nclip();
    gte_stopz(&back);
    SDL_Log("force props check nclip front %ld back %ld", front, back);
    failed = 0;
    failed |= check(front > 0, "front face passes nclip");
    failed |= check(back < 0, "back face fails nclip");
    return failed;
}

static int check_bed(void) {
    MATRIX saved;
    GteSettings settings;
    int failed;

    memset(&saved, 0, sizeof saved);
    gte_ReadRotMatrix(&saved);
    gte_debug_settings(&settings);
    failed = check_nclip_sign();
    gte_SetRotMatrix(&saved);
    gte_SetTransMatrix(&saved);
    SetGeomOffset(settings.off_x, settings.off_y);
    SetGeomScreen(settings.screen_h);
    host_clear();
    return failed;
}

static int read_at(int x, int y, int r, int g, int b);

static int check_bin_picture(void) {
    uint16_t red;
    uint16_t green;
    uint16_t zero;
    char *base;
    char dir[512];
    char path[640];
    int failed;

    failed = 0;
    red = 0x001F;
    green = 0x03E0;
    zero = 0;
    vram_fill(66, 8, 3, 9, zero);
    vram_fill(74, 40, 3, 9, zero);
    vram_load(700, 420, 1, 1, &red);
    bin_picture_test_arm(64, 0, 8, 8, 16, 16, 700, 420, 15, 0, 0, 0, 0, 0, 0, 1);

    farm_area.mode.place.location = PLACE_FARM;
    host_clear();
    draw_key(120, 80, 64, 8, 8, 16, 16, 128, 128, 128, 1);
    failed |= check(read_at(124, 84, 255, 0, 0), "a constructed bin polygon on the farm reads the picture");

    farm_area.mode.place.location = PLACE_4;
    host_clear();
    draw_key(120, 80, 64, 8, 8, 16, 16, 128, 128, 128, 1);
    failed |= check(read_at(124, 84, 255, 0, 0), "a constructed bin polygon in the coop reads the picture");

    farm_area.mode.place.location = PLACE_5;
    host_clear();
    draw_key(120, 80, 64, 8, 8, 16, 16, 128, 128, 128, 1);
    failed |= check(read_at(124, 84, 255, 0, 0), "a constructed bin polygon in the barn reads the picture");

    farm_area.mode.place.location = PLACE_2;
    host_clear();
    draw_key(120, 80, 64, 8, 8, 16, 16, 128, 128, 128, 1);
    failed |= check(read_at(124, 84, 13, 18, 31), "a constructed bin polygon outside those areas keeps the cleared sample");

    farm_area.mode.place.location = PLACE_FARM;
    host_clear();
    draw_key(140, 80, 64, 40, 40, 48, 48, 128, 128, 128, 1);
    failed |= check(read_at(144, 84, 13, 18, 31), "a neighbor polygon keeps the cleared sample");

    vram_load(700, 420, 1, 1, &zero);
    bin_picture_test_arm(64, 0, 8, 8, 16, 16, 700, 420, 15, 0, 0, 0, 0, 0, 0, 0);
    host_clear();
    draw_key(120, 100, 64, 8, 8, 16, 16, 200, 40, 20, 1);
    failed |= check(read_at(124, 104, 13, 18, 31), "a picture cell of word 0x0000 does not paint a substitute color");

    vram_load(700, 420, 1, 1, &green);
    bin_picture_test_arm(64, 0, 8, 8, 16, 16, 700, 420, 15, 0, 0, 0, 0, 0, 0, 1);
    base = SDL_GetBasePath();
    if (base == NULL) {
        bin_picture_test_disarm();
        farm_area.mode.place.location = PLACE_FARM;
        return check(0, "the executable folder is available");
    }
    snprintf(dir, sizeof dir, "%smods-bin-picture", base);
    SDL_free(base);
    _mkdir(dir);
    snprintf(path, sizeof path, "%s/64_0_8_8_16_16.bmp", dir);
    write_red(path);
    snprintf(env_mods, sizeof env_mods, "HMBTN_MODS=%s", dir);
    _putenv(env_mods);
    mods_rescan();
    host_clear();
    draw_key(120, 120, 64, 8, 8, 16, 16, 128, 128, 128, 1);
    failed |= check(read_at(124, 124, 255, 0, 0), "a bmp for the constructed bin polygon reads the bmp color");
    _putenv("HMBTN_MODS=");
    mods_rescan();
    remove(path);
    _rmdir(dir);
    bin_picture_test_disarm();
    farm_area.mode.place.location = PLACE_FARM;
    host_clear();
    return failed;
}

static int read_at(int x, int y, int r, int g, int b) {
    unsigned char rgb[3];

    gpu_debug_read_pixel(x, y, rgb);
    SDL_Log("force props check area pixel %d,%d,%d", rgb[0], rgb[1], rgb[2]);
    return near(rgb[0], r) && near(rgb[1], g) && near(rgb[2], b);
}

int force_props_self_check(void) {
    unsigned char rgb[3];
    char *base;
    char dir[512];
    char path[640];
    uint16_t zero;
    short saved_place;
    int failed;

    failed = 0;
    zero = 0;
    saved_place = farm_area.mode.place.location;
    farm_area.mode.place.location = PLACE_FARM;
    gpu_debug_reset_clear();
    put_word(0, 480, zero);
    vram_fill(380, 236, 4, 13, zero);
    vram_fill(438, 153, 4, 13, zero);
    vram_fill(0, 0, 2, 5, zero);

    host_clear();
    draw_key(20, 20, 320, 240, 236, 252, 248, 128, 128, 128, 0);
    gpu_debug_read_pixel(24, 24, rgb);
    SDL_Log("force props check bin pixel %d,%d,%d", rgb[0], rgb[1], rgb[2]);
    failed |= check(near(rgb[0], 128) && near(rgb[1], 128) && near(rgb[2], 128),
                    "the bin key with word 0x0000 reads 128,128,128");

    host_clear();
    draw_key(40, 20, 0, 0, 0, 4, 4, 128, 128, 128, 0);
    gpu_debug_read_pixel(44, 24, rgb);
    SDL_Log("force props check other pixel %d,%d,%d", rgb[0], rgb[1], rgb[2]);
    failed |= check(near(rgb[0], 13) && near(rgb[1], 18) && near(rgb[2], 31),
                    "the same word on page 0,0 leaves the cleared color");

    host_clear();
    draw_key(60, 20, 384, 216, 153, 228, 165, 40, 80, 160, 0);
    gpu_debug_read_pixel(64, 24, rgb);
    SDL_Log("force props check door pixel %d,%d,%d", rgb[0], rgb[1], rgb[2]);
    failed |= check(near(rgb[0], 40) && near(rgb[1], 80) && near(rgb[2], 160),
                    "the door key with word 0x0000 reads its corner color");

    host_clear();
    draw_key(100, 20, 320, 200, 200, 255, 255, 128, 128, 128, 0);
    failed |= check(read_at(104, 24, 13, 18, 31),
                    "a wider rectangle that contains the bin point leaves the cleared color");

    farm_area.mode.place.location = PLACE_4;
    host_clear();
    draw_key(20, 40, 320, 240, 236, 252, 248, 128, 128, 128, 0);
    failed |= check(read_at(24, 44, 128, 128, 128), "the bin rectangle in the coop reads 128,128,128");

    farm_area.mode.place.location = PLACE_5;
    host_clear();
    draw_key(20, 40, 320, 240, 236, 252, 248, 128, 128, 128, 0);
    failed |= check(read_at(24, 44, 128, 128, 128), "the bin rectangle in the barn reads 128,128,128");

    farm_area.mode.place.location = PLACE_2;
    host_clear();
    draw_key(20, 40, 320, 240, 236, 252, 248, 128, 128, 128, 0);
    failed |= check(read_at(24, 44, 13, 18, 31), "the bin rectangle outside its areas leaves the cleared color");
    host_clear();
    draw_key(60, 40, 384, 216, 153, 228, 165, 40, 80, 160, 0);
    failed |= check(read_at(64, 44, 13, 18, 31), "the door rectangle outside the farm leaves the cleared color");

    farm_area.mode.place.location = PLACE_FARM;
    base = SDL_GetBasePath();
    if (base == NULL) {
        failed |= check(0, "the executable folder is available");
        farm_area.mode.place.location = saved_place;
        host_clear();
        return failed == 0 ? 0 : 1;
    }
    snprintf(dir, sizeof dir, "%smods-force-props", base);
    SDL_free(base);
    _mkdir(dir);
    snprintf(path, sizeof path, "%s/320_0_240_236_252_248.bmp", dir);
    write_red(path);
    snprintf(env_mods, sizeof env_mods, "HMBTN_MODS=%s", dir);
    _putenv(env_mods);
    mods_rescan();

    host_clear();
    draw_key(80, 20, 320, 240, 236, 252, 248, 128, 128, 128, 1);
    gpu_debug_read_pixel(84, 24, rgb);
    SDL_Log("force props check bmp pixel %d,%d,%d", rgb[0], rgb[1], rgb[2]);
    failed |= check(rgb[0] > 200 && rgb[1] < 40 && rgb[2] < 40, "a bmp for the bin key reads the bmp color");

    _putenv("HMBTN_MODS=");
    mods_rescan();
    remove(path);
    _rmdir(dir);
    host_clear();
    failed |= check_bin_picture();
    failed |= check_bed();
    farm_area.mode.place.location = saved_place;
    return failed == 0 ? 0 : 1;
}
