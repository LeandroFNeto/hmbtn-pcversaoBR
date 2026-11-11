/* Measures blend rate 2 with a white source. A black source cannot tell
 * subtraction from addition. The house and flashback frames are recorded later. */

#include <bg_trace.h>
#include <host.h>
#include <libgpu.h>

#include <SDL.h>

#include <string.h>

static DR_TPAGE bg_tpage;
static POLY_F4 bg_quad;

static int check(int ok, const char *what) {
    SDL_Log("bgtrace check %s: %s", ok ? "ok" : "FAILED", what);
    return ok ? 0 : 1;
}

static int black(const unsigned char rgb[3]) {
    return rgb[0] <= 8 && rgb[1] <= 8 && rgb[2] <= 8;
}

static void draw_white_rate2(int x, int y) {
    memset(&bg_tpage, 0, sizeof(bg_tpage));
    memset(&bg_quad, 0, sizeof(bg_quad));
    setDrawTPage(&bg_tpage, 0, 0, getTPage(2, 2, 0, 0));
    setPolyF4(&bg_quad);
    setSemiTrans(&bg_quad, 1);
    setRGB0(&bg_quad, 255, 255, 255);
    setXY4(&bg_quad, x, y, x + 8, y, x, y + 8, x + 8, y + 8);
    catPrim(&bg_tpage, &bg_quad);
    termPrim(&bg_quad);
    DrawOTag((u_long *)&bg_tpage);
}

static int check_rate2(void) {
    unsigned char over_white[3];
    unsigned char over_dark[3];
    int failed;

    gpu_clear_screen(255, 255, 255);
    draw_white_rate2(20, 20);
    gpu_debug_read_pixel(24, 24, over_white);
    gpu_clear_screen(40, 0, 0);
    draw_white_rate2(20, 20);
    gpu_debug_read_pixel(24, 24, over_dark);
    SDL_Log("bgtrace check rate2 white-over-white %d,%d,%d white-over-40 %d,%d,%d", over_white[0], over_white[1],
            over_white[2], over_dark[0], over_dark[1], over_dark[2]);
    failed = 0;
    failed |= check(black(over_white), "rate 2 white over white reads 0,0,0");
    failed |= check(black(over_dark), "rate 2 white over 40,0,0 reads 0,0,0");
    gpu_debug_reset_clear();
    host_clear();
    return failed;
}

typedef struct {
    u_long tag;
    u_long word;
} BgWord;

typedef struct {
    u_long tag;
    u_long color;
    u_long xy;
    u_long size;
} BgFill;

static int check_record(void) {
    static POLY_F4 small;
    static DR_TPAGE tpage;
    static POLY_F4 large;
    static BgWord e3;
    static BgWord e4;
    static BgWord e5;
    static BgWord e6;
    static BgFill fill;
    RECT rect;
    const BgTraceView *view;
    unsigned char rgb[3];
    int skips;
    int edges;
    int logged;
    int failed;

    logged = bg_trace_logged_clears();
    setRECT(&rect, 1, 1, 2, 2);
    ClearImage(&rect, 1, 2, 3);
    failed = check(bg_trace_logged_clears() == logged, "a clear with the flag off is not logged");

    bg_trace_force(1);
    bg_trace_reset();
    gpu_clear_screen(40, 0, 0);

    memset(&small, 0, sizeof(small));
    setPolyF4(&small);
    setSemiTrans(&small, 0);
    setRGB0(&small, 10, 0, 0);
    setXY4(&small, 156, 116, 164, 116, 156, 124, 164, 124);

    memset(&tpage, 0, sizeof(tpage));
    setDrawTPage(&tpage, 0, 0, getTPage(2, 2, 0, 0));

    memset(&large, 0, sizeof(large));
    setPolyF4(&large);
    setSemiTrans(&large, 1);
    setRGB0(&large, 255, 255, 255);
    setXY4(&large, 80, 80, 240, 80, 80, 200, 240, 200);

    memset(&e3, 0, sizeof(e3));
    setlen(&e3, 1);
    e3.word = 0xE300040Au;
    memset(&e4, 0, sizeof(e4));
    setlen(&e4, 1);
    e4.word = 0xE4000001u;
    memset(&e5, 0, sizeof(e5));
    setlen(&e5, 1);
    e5.word = 0xE5000002u;
    memset(&e6, 0, sizeof(e6));
    setlen(&e6, 1);
    e6.word = 0xE6000001u;
    memset(&fill, 0, sizeof(fill));
    setlen(&fill, 3);
    fill.color = 0x0200FF00u;
    fill.xy = 300u | (220u << 16);
    fill.size = 4u | (4u << 16);

    catPrim(&small, &tpage);
    catPrim(&tpage, &large);
    catPrim(&large, &e3);
    catPrim(&e3, &e4);
    catPrim(&e4, &e5);
    catPrim(&e5, &e6);
    catPrim(&e6, &fill);
    termPrim(&fill);
    DrawOTag((u_long *)&small);
    gpu_debug_read_pixel(302, 222, rgb);
    failed |= check(rgb[0] >= 32 && rgb[0] <= 48 && rgb[1] <= 8, "command 0x02 is not drawn");

    setRECT(&rect, 4, 6, 12, 14);
    ClearImage(&rect, 9, 8, 7);
    skips = bg_trace_skip_lines();
    edges = bg_trace_edge_lines();
    bg_trace_emit(0, 0x50);
    view = bg_trace_last();
    failed |= check(view->point[4].none == 0 && view->point[4].r == 255 && view->point[4].g == 255 &&
                        view->point[4].b == 255 && view->point[4].semi == 1 && view->point[4].abr == 2 &&
                        view->point[4].cmd == 0x2a,
                    "the later primitive at 160,120 keeps its own file color");
    failed |= check(view->point[4].clear_after == 1, "a window clear after the center primitive says clear after");
    failed |= check(view->point[4].pixel_r < 40, "naming rgb 255,255,255 does not recolor the pixel");
    failed |= check(view->point[0].none == 1, "40,40 is none");
    failed |= check(view->large_count == 1 && view->large[0].r == 255 && view->large[0].x1 - view->large[0].x0 >= 160,
                    "the large quad is listed and the small quad is not");
    failed |= check(view->clear_count == 1 && view->clear[0].x == 4 && view->clear[0].y == 6 && view->clear[0].w == 12 &&
                        view->clear[0].h == 14 && view->clear[0].r == 9 && view->clear[0].g == 8 &&
                        view->clear[0].b == 7 && view->clear[0].window == 1,
                    "the clear rectangle is named");
    failed |= check(view->e6_count == 1 && view->e6_word == 0xE6000001u && view->color_r == 1 && view->color_g == 1 &&
                        view->color_b == 1 && view->color_a == 1,
                    "the mask word is named and the color write mask is unchanged");
    failed |= check(view->e3_count == 1 && view->e3_word == 0xE300040Au && view->e4_count == 1 &&
                        view->e4_word == 0xE4000001u && view->e5_count == 1 && view->e5_word == 0xE5000002u &&
                        view->fill_count == 1,
                    "draw-area words and the fill are recorded");
    failed |= check(bg_trace_edge_lines() == edges + 5, "the house frame writes five edge lines");
    bg_trace_emit(1, 0);
    failed |= check(bg_trace_skip_lines() == skips + 1, "another scene writes one skip line");
    failed |= check(bg_trace_edge_lines() == edges + 5, "another scene writes no edge block");
    gpu_debug_reset_clear();
    host_clear();
    return failed;
}

int bg_trace_self_check(void) {
    int failed;

    bg_trace_force(-1);
    failed = check_rate2();
    failed |= check_record();
    bg_trace_force(0);
    bg_trace_reset();
    return failed == 0 ? 0 : 1;
}
