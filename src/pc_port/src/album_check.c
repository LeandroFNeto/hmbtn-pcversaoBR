/* Album capture. A copy from the draw buffer into (896, 256) writes the pixels
 * just drawn. Run at startup when HMBTN_SELFTEST=1. */

#include <host.h>
#include <libgpu.h>
#include <vram.h>

#include <SDL.h>

#include <stdlib.h>
#include <string.h>

static TILE album_tile;
static DR_MOVE album_move;
static DR_TPAGE album_tpage;
static SPRT album_sprt;
static uint16_t album_photo[8];

static int check(int ok, const char *what) {
    SDL_Log("album check %s: %s", ok ? "ok" : "FAILED", what);
    return ok ? 0 : 1;
}

static int block_is(int x, int y, int w, int h, uint16_t word) {
    int row;
    int col;

    for (row = 0; row < h; row++) {
        for (col = 0; col < w; col++) {
            if (vram[(y + row) & (VRAM_HEIGHT - 1)][(x + col) & (VRAM_WIDTH - 1)] != word) {
                return 0;
            }
        }
    }
    return 1;
}

static void draw_tile(int x, int y, int r, int g, int b) {
    memset(&album_tile, 0, sizeof album_tile);
    setTile(&album_tile);
    setRGB0(&album_tile, r, g, b);
    setXY0(&album_tile, x, y);
    setWH(&album_tile, 8, 8);
    termPrim(&album_tile);
    DrawOTag((u_long *)&album_tile);
}

static int pixel_is(int x, int y, int r, int g, int b) {
    unsigned char rgb[3];

    gpu_debug_read_pixel(x, y, rgb);
    return abs(rgb[0] - r) <= 8 && abs(rgb[1] - g) <= 8 && abs(rgb[2] - b) <= 8;
}

int album_self_check(void) {
    RECT r;
    int failed;

    gpu_put_draw_env(0, 0, 0, 0, 0, 0, 0, 320, 240);
    vram[400][896] = 0x5555;
    draw_tile(16, 16, 255, 0, 0);
    failed = check(vram[400][896] == 0x5555, "a drawn frame with no album copy leaves the sentinel");

    setRECT(&r, 16, 16, 8, 8);
    MoveImage(&r, 896, 256);
    failed |= check(block_is(896, 256, 8, 8, vram_color15(255, 0, 0)),
                    "an 8 x 8 move from the draw buffer lands that color at (896, 256)");

    memset(&album_tile, 0, sizeof album_tile);
    setTile(&album_tile);
    setRGB0(&album_tile, 0, 255, 0);
    setXY0(&album_tile, 48, 48);
    setWH(&album_tile, 8, 8);
    setRECT(&r, 48, 48, 8, 8);
    SetDrawMove(&album_move, &r, 896, 256);
    catPrim(&album_tile, &album_move);
    termPrim(&album_move);
    DrawOTag((u_long *)&album_tile);
    failed |= check(block_is(896, 256, 8, 8, vram_color15(0, 255, 0)),
                    "ordering-table 0x80 lands that color at (896, 256)");

    gpu_put_draw_env(0, 0, 0, 0, 0, 0, 240, 320, 240);
    gpu_clear_screen(0, 0, 255);
    setRECT(&r, 20, 260, 8, 8);
    MoveImage(&r, 896, 256);
    failed |= check(block_is(896, 256, 8, 8, vram_color15(0, 0, 255)),
                    "a move from the buffer at (0, 240) lands the pixels of that frame");

    gpu_put_draw_env(10, 0, 0, 0, 0, 0, 0, 320, 240);
    gpu_debug_reset_clear();
    host_clear();
    memset(&album_tpage, 0, sizeof album_tpage);
    setDrawTPage(&album_tpage, 0, 0, getTPage(2, 0, 896, 256));
    memset(&album_sprt, 0, sizeof album_sprt);
    setSprt(&album_sprt);
    setShadeTex(&album_sprt, 1);
    setRGB0(&album_sprt, 128, 128, 128);
    setXY0(&album_sprt, 100, 100);
    setUV0(&album_sprt, 0, 0);
    setWH(&album_sprt, 1, 1);
    catPrim(&album_tpage, &album_sprt);
    termPrim(&album_sprt);
    DrawOTag((u_long *)&album_tpage);
    failed |= check(pixel_is(100, 100, 0, 0, 255), "a 16-bit sprite of (896, 256) draws the captured color");

    vram_fill(400, 100, 8, 8, 0xABCD);
    setRECT(&r, 400, 100, 8, 8);
    MoveImage(&r, 896, 256);
    failed |= check(block_is(896, 256, 8, 8, 0xABCD),
                    "a move from outside the draw buffer keeps the source word");

    vram_fill(200, 10, 16, 1, 0x2222);
    draw_tile(200, 10, 0, 0, 255);
    setRECT(&r, 200, 10, 16, 1);
    MoveImage(&r, 896, 10);
    failed |= check(block_is(896, 10, 16, 1, 0x2222),
                    "a 16 x 1 move at x = 896 and y = 10 keeps the source word");

    vram_fill(896, 256, 64, 256, 0x1234);
    setRECT(&r, 896, 256, 8, 1);
    memset(album_photo, 0, sizeof album_photo);
    StoreImage(&r, (u_long *)album_photo);
    failed |= check(album_photo[0] == 0x1234 && album_photo[7] == 0x1234,
                    "storing the scratch with no capture returns the word already there");

    host_clear();
    return failed;
}
