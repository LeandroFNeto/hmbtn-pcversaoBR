/* VRAM, sampling, walker, and blend checks, run at startup when HMBTN_SELFTEST=1. They
 * draw into the back buffer and read it back before any swap. */

#include <gte.h>
#include <host.h>
#include <libgpu.h>
#include <vram.h>

#include <SDL.h>

#include <stdlib.h>
#include <string.h>

static int observed;
static int resolved;

static int check(int ok, const char *what) {
    SDL_Log("vram check %s: %s", ok ? "ok" : "FAILED", what);
    return ok ? 0 : 1;
}

static void count_observer(int x, int y, int w, int h, const void *source, VramWriteKind kind) {
    (void)x;
    (void)y;
    (void)w;
    (void)h;
    (void)source;
    (void)kind;
    observed++;
}

static TextureSource count_resolver(int page_x, int page_y, int depth, int clut_x, int clut_y, int u0, int v0,
                                    int u1, int v1) {
    (void)page_x;
    (void)page_y;
    (void)depth;
    (void)clut_x;
    (void)clut_y;
    (void)u0;
    (void)v0;
    (void)u1;
    (void)v1;
    resolved++;
    return TEXTURE_FROM_VRAM;
}

static int near(int value, int expected, int tolerance) {
    return abs(value - expected) <= tolerance;
}

static int is_rgb(int x, int y, int r, int g, int b, int tolerance) {
    unsigned char rgb[3];

    gpu_debug_read_pixel(x, y, rgb);
    return near(rgb[0], r, tolerance) && near(rgb[1], g, tolerance) && near(rgb[2], b, tolerance);
}

static void put_word(int x, int y, uint16_t value) {
    vram_load(x, y, 1, 1, &value);
}

/* Packets used by the draw checks. Static, so their 24-bit links stay in the image. */
static DR_TPAGE ck_tpage;
static DR_LOAD ck_load;
static SPRT ck_sprt;
static SPRT_8 ck_sprt8;
static TILE ck_tile;
static TILE_16 ck_tile16;
static TILE_1 ck_tile1;
static POLY_F4 ck_f4;
static POLY_G3 ck_g3;
static POLY_FT3 ck_ft3;
static POLY_FT4 ck_ft4;
static POLY_GT4 ck_gt4;
static POLY_G4 ck_g4;
static DR_TWIN ck_twin;
static DR_MODE ck_mode;

static void draw_one(void *p) {
    termPrim(p);
    DrawOTag((u_long *)p);
}

static void draw_after_tpage(int tpage, void *p) {
    memset(&ck_tpage, 0, sizeof(ck_tpage));
    setDrawTPage(&ck_tpage, 0, 0, tpage);
    catPrim(&ck_tpage, p);
    termPrim(p);
    DrawOTag((u_long *)&ck_tpage);
}

static void make_sprt(int x, int y, int w, int h, int u, int clut, int raw) {
    memset(&ck_sprt, 0, sizeof(ck_sprt));
    setSprt(&ck_sprt);
    setShadeTex(&ck_sprt, raw);
    setRGB0(&ck_sprt, 128, 128, 128);
    setXY0(&ck_sprt, x, y);
    setUV0(&ck_sprt, u, 0);
    setWH(&ck_sprt, w, h);
    ck_sprt.clut = (u_short)clut;
}

static int check_transfers(void) {
    uint16_t in[32];
    uint16_t out[32];
    uint16_t row[8];
    RECT r;
    DR_LOAD load;
    int failed;
    int i;

    for (i = 0; i < 32; i++) {
        in[i] = (uint16_t)(0x1000 + i);
    }
    setRECT(&r, 640, 0, 16, 2);
    LoadImage(&r, (u_long *)in);
    memset(out, 0, sizeof(out));
    StoreImage(&r, (u_long *)out);
    failed = check(memcmp(in, out, sizeof(in)) == 0, "16 x 2 load at (640, 0) stores back equal");

    setRECT(&r, 1020, 10, 8, 1);
    LoadImage(&r, (u_long *)in);
    failed |= check(vram[10][1023] == in[3] && vram[10][0] == in[4] && vram[10][3] == in[7],
                    "a load at x = 1020, width 8, wraps to x = 0");

    for (i = 0; i < 8; i++) {
        row[i] = (uint16_t)(0x2000 + i);
    }
    for (i = 0; i < 8; i++) {
        vram_load(512, 100 + i, 8, 1, row);
    }
    setRECT(&r, 512, 100, 8, 8);
    MoveImage(&r, 516, 100);
    failed |= check(vram[100][516] == 0x2000 && vram[107][523] == 0x2007 && vram[100][512] == 0x2000,
                    "an overlapping 8 x 8 move keeps the source's original words");

    setRECT(&r, 512, 300, 4, 4);
    ClearImage(&r, 255, 0, 0);
    failed |= check(vram[300][512] == 0x001F && vram[303][515] == 0x001F, "clearing to (255, 0, 0) writes 0x001F");

    observed = 0;
    vram_set_observer(count_observer);
    setRECT(&r, 512, 310, 2, 1);
    LoadImage(&r, (u_long *)in);
    ClearImage(&r, 0, 0, 0);
    MoveImage(&r, 600, 310);
    vram_set_observer(NULL);
    failed |= check(observed == 3, "the observer sees one call per LoadImage, ClearImage and MoveImage");

    setRECT(&r, 0, 480, 16, 1);
    SetDrawLoad(&load, &r);
    failed |= check((load.code[0] >> 24) == 0xA0 && load.code[1] == (480u << 16) && load.code[2] == ((1u << 16) | 16u) &&
                        getlen(&load) == 3 + 8,
                    "SetDrawLoad writes 0xA0, the rectangle, and length 3 + 8");
    return failed;
}

static int check_sampling(void) {
    int tp4 = getTPage(0, 0, 640, 0);
    int tp8 = getTPage(1, 0, 704, 0);
    int tp15 = getTPage(2, 0, 768, 0);
    int clut4 = getClut(0, 480);
    int clut8 = getClut(0, 481);
    int failed;

    put_word(640, 0, 0x0001);                 /* 4-bit: texel 0 = index 1, texels 1-3 = index 0 */
    put_word(0, 480, 0x0000);
    put_word(1, 480, 0x001F);                 /* CLUT entry 1: red */
    put_word(704, 0, 0x0002);                 /* 8-bit: texel 0 = index 2, texel 1 = index 0 */
    put_word(2, 481, 0x03E0);                 /* CLUT entry 2: green */
    put_word(768, 0, 0x7C00);                 /* 15-bit: blue */
    put_word(769, 0, 0x7FFF);                 /* 15-bit: white */

    host_clear();
    make_sprt(10, 10, 4, 1, 0, clut4, 1);
    draw_after_tpage(tp4, &ck_sprt);
    failed = check(is_rgb(10, 10, 255, 0, 0, 8), "a raw 4-bit SPRT whose index 1 maps to red draws red");
    failed |= check(is_rgb(12, 10, 13, 18, 31, 8), "a 0x0000 texel leaves the cleared color");

    make_sprt(20, 10, 1, 1, 0, clut8, 1);
    draw_after_tpage(tp8, &ck_sprt);
    failed |= check(is_rgb(20, 10, 0, 255, 0, 8), "an 8-bit sprite draws its CLUT color");

    make_sprt(30, 10, 1, 1, 0, 0, 1);
    draw_after_tpage(tp15, &ck_sprt);
    failed |= check(is_rgb(30, 10, 0, 0, 255, 8), "a 15-bit sprite draws its word's color");

    make_sprt(40, 10, 1, 1, 1, 0, 0);
    setRGB0(&ck_sprt, 64, 64, 64);
    draw_after_tpage(tp15, &ck_sprt);
    failed |= check(is_rgb(40, 10, 128, 128, 128, 10), "color (64, 64, 64) halves a white texel");
    host_clear();
    return failed;
}

static int check_primitives(void) {
    int tp15 = getTPage(2, 0, 768, 0);
    int failed;
    int y;

    for (y = 0; y < 16; y++) {
        uint16_t white[16];
        int i;

        for (i = 0; i < 16; i++) {
            white[i] = 0x7FFF;
        }
        vram_load(784, y, 16, 1, white);      /* 15-bit white block at page u = 16..31 */
    }
    host_clear();

    memset(&ck_g3, 0, sizeof(ck_g3));
    setPolyG3(&ck_g3);
    setRGB0(&ck_g3, 255, 0, 0);
    setRGB1(&ck_g3, 255, 0, 0);
    setRGB2(&ck_g3, 255, 0, 0);
    setXY3(&ck_g3, 50, 50, 70, 50, 50, 70);
    draw_one(&ck_g3);
    failed = check(is_rgb(54, 54, 255, 0, 0, 8), "a POLY_G3 draws its vertex colors");

    memset(&ck_ft3, 0, sizeof(ck_ft3));
    setPolyFT3(&ck_ft3);
    setShadeTex(&ck_ft3, 1);
    setXY3(&ck_ft3, 80, 50, 96, 50, 80, 66);
    setUV3(&ck_ft3, 16, 0, 31, 0, 16, 15);
    ck_ft3.tpage = (u_short)tp15;
    draw_one(&ck_ft3);
    failed |= check(is_rgb(83, 53, 255, 255, 255, 8), "a POLY_FT3 samples its own page");

    memset(&ck_gt4, 0, sizeof(ck_gt4));
    setPolyGT4(&ck_gt4);
    setRGB0(&ck_gt4, 128, 0, 0);
    setRGB1(&ck_gt4, 128, 0, 0);
    setRGB2(&ck_gt4, 128, 0, 0);
    setRGB3(&ck_gt4, 128, 0, 0);
    setXY4(&ck_gt4, 110, 50, 126, 50, 110, 66, 126, 66);
    setUV4(&ck_gt4, 16, 0, 31, 0, 16, 15, 31, 15);
    ck_gt4.tpage = (u_short)tp15;
    draw_one(&ck_gt4);
    failed |= check(is_rgb(118, 58, 255, 0, 0, 10), "a POLY_GT4 modulates its texels by vertex color");

    memset(&ck_tile16, 0, sizeof(ck_tile16));
    setTile16(&ck_tile16);
    setRGB0(&ck_tile16, 0, 0, 255);
    setXY0(&ck_tile16, 140, 50);
    draw_one(&ck_tile16);
    failed |= check(is_rgb(155, 65, 0, 0, 255, 8) && !is_rgb(157, 67, 0, 0, 255, 8), "a TILE_16 covers 16 x 16");

    memset(&ck_tile1, 0, sizeof(ck_tile1));
    setTile1(&ck_tile1);
    setRGB0(&ck_tile1, 255, 255, 0);
    setXY0(&ck_tile1, 170, 50);
    draw_one(&ck_tile1);
    failed |= check(is_rgb(170, 50, 255, 255, 0, 8) && !is_rgb(171, 50, 255, 255, 0, 8), "a TILE_1 covers one pixel");

    memset(&ck_sprt8, 0, sizeof(ck_sprt8));
    setSprt8(&ck_sprt8);
    setShadeTex(&ck_sprt8, 1);
    setXY0(&ck_sprt8, 190, 50);
    setUV0(&ck_sprt8, 16, 0);
    memset(&ck_tpage, 0, sizeof(ck_tpage));
    setDrawTPage(&ck_tpage, 0, 0, tp15);
    catPrim(&ck_tpage, &ck_sprt8);
    termPrim(&ck_sprt8);
    DrawOTag((u_long *)&ck_tpage);
    failed |= check(is_rgb(197, 57, 255, 255, 255, 8) && !is_rgb(199, 59, 255, 255, 255, 8), "an SPRT_8 covers 8 x 8");
    host_clear();
    return failed;
}

static int check_chain_state(void) {
    int tp4 = getTPage(0, 0, 640, 0);
    int clut4 = getClut(0, 480);
    RECT r;
    uint16_t *data;
    int failed;
    int i;

    host_clear();
    make_sprt(10, 30, 1, 1, 0, clut4, 1);
    draw_after_tpage(tp4, &ck_sprt);
    failed = check(is_rgb(10, 30, 255, 0, 0, 8), "a DR_TPAGE (page 640, 4-bit) then SPRT samples that page");

    /* DR_LOAD a new CLUT row whose entry 1 is green, then the same sprite. */
    setRECT(&r, 0, 480, 16, 1);
    memset(&ck_load, 0, sizeof(ck_load));
    SetDrawLoad(&ck_load, &r);
    data = (uint16_t *)ck_load.p;
    for (i = 0; i < 16; i++) {
        data[i] = 0;
    }
    data[1] = 0x03E0;
    memset(&ck_tpage, 0, sizeof(ck_tpage));
    setDrawTPage(&ck_tpage, 0, 0, tp4);
    make_sprt(12, 30, 1, 1, 0, clut4, 1);
    catPrim(&ck_load, &ck_tpage);
    catPrim(&ck_tpage, &ck_sprt);
    termPrim(&ck_sprt);
    DrawOTag((u_long *)&ck_load);
    failed |= check(is_rgb(12, 30, 0, 255, 0, 8), "a DR_LOAD of a CLUT row then SPRT draws the new colors");
    put_word(1, 480, 0x001F);
    host_clear();
    return failed;
}

static void tile(int x, int y, int r, int g, int b, int semi) {
    memset(&ck_tile, 0, sizeof(ck_tile));
    setTile(&ck_tile);
    setRGB0(&ck_tile, r, g, b);
    setXY0(&ck_tile, x, y);
    setWH(&ck_tile, 8, 8);
    setSemiTrans(&ck_tile, semi);
}

static int check_blending(void) {
    int failed;

    gpu_clear_screen(0, 0, 0);

    tile(10, 100, 255, 255, 255, 1);
    draw_after_tpage(getTPage(2, 0, 768, 0), &ck_tile);
    failed = check(is_rgb(12, 102, 128, 128, 128, 10), "a mode-0 white TILE over black is about 50% gray");

    tile(30, 100, 64, 64, 64, 0);
    draw_one(&ck_tile);
    tile(30, 100, 64, 64, 64, 1);
    draw_after_tpage(getTPage(2, 1, 768, 0), &ck_tile);
    failed |= check(is_rgb(32, 102, 128, 128, 128, 10), "a mode-1 gray over gray adds");

    tile(50, 100, 255, 0, 0, 0);
    draw_one(&ck_tile);
    memset(&ck_f4, 0, sizeof(ck_f4));
    setPolyF4(&ck_f4);
    setRGB0(&ck_f4, 0, 0, 0);
    setXY4(&ck_f4, 48, 98, 62, 98, 48, 112, 62, 112);
    setSemiTrans(&ck_f4, 1);
    draw_after_tpage(getTPage(2, 2, 768, 0), &ck_f4);
    failed |= check(is_rgb(52, 102, 255, 0, 0, 8), "a mode-2 black POLY_F4 leaves the frame unchanged");

    put_word(770, 0, 0xFFFF);                 /* white with bit 15 */
    put_word(771, 0, 0x7FFF);                 /* white without */
    make_sprt(70, 100, 2, 1, 2, 0, 1);
    setSemiTrans(&ck_sprt, 1);
    draw_after_tpage(getTPage(2, 0, 768, 0), &ck_sprt);
    failed |= check(is_rgb(70, 100, 128, 128, 128, 10) && is_rgb(71, 100, 255, 255, 255, 8),
                    "a textured mode-0 sprite blends only its texels with bit 15 set");

    {
        RECT r;

        setRECT(&r, 0, 0, 320, 480);
        ClearImage(&r, 0, 0, 255);
        host_clear();
        failed |= check(is_rgb(5, 5, 0, 0, 255, 8), "clearing (0, 0, 320, 480) to blue makes the next frame blue");
    }
    gpu_debug_reset_clear();
    host_clear();
    return failed;
}

static int check_resolver(void) {
    int tp4 = getTPage(0, 0, 640, 0);
    int failed;

    resolved = 0;
    vram_set_resolver(count_resolver);
    memset(&ck_f4, 0, sizeof(ck_f4));
    setPolyF4(&ck_f4);
    setXY4(&ck_f4, 0, 0, 4, 0, 0, 4, 4, 4);
    memset(&ck_ft4, 0, sizeof(ck_ft4));
    setPolyFT4(&ck_ft4);
    setXY4(&ck_ft4, 0, 0, 4, 0, 0, 4, 4, 4);
    ck_ft4.tpage = (u_short)tp4;
    make_sprt(0, 0, 4, 4, 0, getClut(0, 480), 1);
    memset(&ck_tpage, 0, sizeof(ck_tpage));
    setDrawTPage(&ck_tpage, 0, 0, tp4);
    catPrim(&ck_f4, &ck_ft4);
    catPrim(&ck_ft4, &ck_tpage);
    catPrim(&ck_tpage, &ck_sprt);
    termPrim(&ck_sprt);
    DrawOTag((u_long *)&ck_f4);
    vram_set_resolver(NULL);
    failed = check(resolved == 2, "the resolver sees each textured primitive once and no untextured one");
    host_clear();
    return failed;
}

static int window_axis(int c, int mask, int off) {
    int bit;

    c &= 255;
    for (bit = 8; bit <= 128; bit <<= 1) {
        if ((mask & bit) != 0) {
            c = (c & ~bit) | (off & bit);
        }
    }
    return c;
}

/* Back-color independence, the texture window, and draw-buffer readback. */
static int check_gpu_fixes(void) {
    RECT tw;
    RECT r;
    uint16_t block[64];
    uint16_t clut[16];
    static uint16_t photo[64 * 256];
    static uint16_t lower[320 * 240];
    int failed;
    int i;
    int mask;
    int off;
    unsigned word;

    failed = check(gpu_debug_vert_bytes() == 56 && gpu_debug_vert_cap() == 6144,
                   "the vertex is 56 bytes and the buffer holds 6144");

    gte_SetBackColor(10, 20, 30);
    host_clear();
    memset(&ck_g4, 0, sizeof(ck_g4));
    setPolyG4(&ck_g4);
    setRGB0(&ck_g4, 0, 255, 0);
    setRGB1(&ck_g4, 0, 255, 0);
    setRGB2(&ck_g4, 0, 255, 0);
    setRGB3(&ck_g4, 0, 255, 0);
    setXY4(&ck_g4, 200, 80, 216, 80, 200, 96, 216, 96);
    draw_one(&ck_g4);
    failed |= check(is_rgb(204, 84, 0, 255, 0, 8),
                    "a POLY_G4 keeps its corner color when the back color differs");
    gte_SetBackColor(0, 0, 0);

    setRECT(&tw, 0, 0, 8, 0);
    SetTexWindow(&ck_twin, &tw);
    word = ck_twin.code[0];
    mask = (int)((word & 0x1F) << 3);
    off = (int)(((word >> 10) & 0x1F) << 3);
    failed |= check(window_axis(9, mask, off) == 1, "mask 8, offset 0 encodes the word whose sample of U 9 is U 1");
    SetDrawMode(&ck_mode, 0, 0, 0, NULL);
    failed |= check(ck_mode.code[1] == 0, "a null SetDrawMode window does not emit 0xE2");

    put_word(640, 0, 0x0020); /* 4-bit texel 1 = index 2 */
    put_word(642, 0, 0x0010); /* 4-bit texel 9 = index 1 */
    put_word(1, 480, 0x001F); /* index 1 red */
    put_word(2, 480, 0x7C00); /* index 2 blue */
    host_clear();
    memset(&ck_tpage, 0, sizeof(ck_tpage));
    setDrawTPage(&ck_tpage, 0, 0, getTPage(0, 0, 640, 0));
    make_sprt(220, 120, 1, 1, 9, getClut(0, 480), 1);
    catPrim(&ck_twin, &ck_tpage);
    catPrim(&ck_tpage, &ck_sprt);
    termPrim(&ck_sprt);
    DrawOTag((u_long *)&ck_twin);
    failed |= check(is_rgb(220, 120, 0, 0, 255, 8), "a windowed sample of U 9 draws the texel at U 1");
    make_sprt(221, 120, 1, 1, 9, getClut(0, 480), 1);
    draw_one(&ck_sprt);
    failed |= check(is_rgb(221, 120, 0, 0, 255, 8), "the next DrawOTag keeps the texture window");
    setRECT(&tw, 0, 0, 0, 0);
    SetTexWindow(&ck_twin, &tw);
    termPrim(&ck_twin);
    DrawOTag((u_long *)&ck_twin);

    host_clear();
    tile(16, 16, 255, 0, 0, 0);
    draw_one(&ck_tile);
    setRECT(&r, 16, 16, 8, 8);
    memset(block, 0, sizeof(block));
    StoreImage(&r, (u_long *)block);
    failed |= check(block[9] == 0x001F && block[54] == 0x001F, "an 8 x 8 block inside the draw buffer stores its color");
    memset(block, 0, sizeof(block));
    StoreImage(&r, (u_long *)block);
    failed |= check(block[9] == 0x001F && block[54] == 0x001F, "storing that block again without drawing returns it");

    for (i = 0; i < 16; i++) {
        clut[i] = 0x1111;
    }
    vram_load(100, 500, 16, 1, clut);
    setRECT(&r, 100, 500, 16, 1);
    memset(clut, 0, sizeof(clut));
    StoreImage(&r, (u_long *)clut);
    failed |= check(clut[0] == 0x1111 && clut[15] == 0x1111, "a 16 x 1 CLUT row outside the draw buffer is unchanged");

    vram_fill(896, 256, 64, 256, 0x1234);
    setRECT(&r, 896, 256, 64, 256);
    StoreImage(&r, (u_long *)photo);
    failed |= check(photo[0] == 0x1234 && photo[64 * 256 - 1] == 0x1234,
                    "the photo scratch (896, 256, 64, 256) is not replaced by the frame");

    vram_fill(0, 240, 320, 240, 0x7FFF);
    gpu_put_draw_env(0, 0, 0, 0, 0, 0, 240, 320, 240);
    gpu_clear_screen(255, 0, 0);
    setRECT(&r, 0, 240, 320, 240);
    StoreImage(&r, (u_long *)lower);
    failed |= check(lower[0] == 0x001F && lower[320 * 120 + 160] == 0x001F && lower[320 * 240 - 1] == 0x001F,
                    "storing (0, 240, 320, 240) returns the frame drawn into that buffer");
    gpu_put_draw_env(10, 0, 0, 0, 0, 0, 0, 320, 240);
    gpu_debug_reset_clear();
    host_clear();
    return failed;
}

int vram_self_check(void) {
    int failed;

    failed = check_transfers();
    failed |= check_sampling();
    failed |= check_primitives();
    failed |= check_chain_state();
    failed |= check_blending();
    failed |= check_resolver();
    failed |= check_gpu_fixes();
    host_debug_reset_game_drawn();
    gpu_take_game_drew();
    return failed;
}
