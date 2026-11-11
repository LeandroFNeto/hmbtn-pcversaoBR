/* Measures the untextured semi quad and the nclip sign. The farm, the bed, and the
 * flashback themselves are logged from a played frame, not from this check. */

#include <gte.h>
#include <host.h>
#include <libgpu.h>

#include <SDL.h>

#include <stdlib.h>
#include <string.h>

static DR_TPAGE sc_tpage;
static POLY_F4 sc_f4;

static int check(int ok, const char *what) {
    SDL_Log("scenery check %s: %s", ok ? "ok" : "FAILED", what);
    return ok ? 0 : 1;
}

static int near(int value, int expected, int tolerance) {
    return abs(value - expected) <= tolerance;
}

static void read_rgb(int x, int y, unsigned char rgb[3]) {
    gpu_debug_read_pixel(x, y, rgb);
}

static void draw_quad(int semi, int x, int y, int r, int g, int b) {
    memset(&sc_tpage, 0, sizeof(sc_tpage));
    memset(&sc_f4, 0, sizeof(sc_f4));
    setDrawTPage(&sc_tpage, 0, 0, getTPage(2, 0, 0, 0));
    setPolyF4(&sc_f4);
    setSemiTrans(&sc_f4, semi);
    setRGB0(&sc_f4, r, g, b);
    setXY4(&sc_f4, x, y, x + 8, y, x, y + 8, x + 8, y + 8);
    catPrim(&sc_tpage, &sc_f4);
    termPrim(&sc_f4);
    DrawOTag((u_long *)&sc_tpage);
}

static int check_blend(void) {
    unsigned char semi[3];
    unsigned char opaque[3];
    int failed;
    int blended;

    gpu_clear_screen(255, 255, 255);
    draw_quad(1, 20, 20, 20, 0, 0);
    draw_quad(0, 40, 40, 20, 0, 0);
    read_rgb(24, 24, semi);
    read_rgb(44, 44, opaque);
    SDL_Log("scenery check semi pixel %d,%d,%d opaque pixel %d,%d,%d", semi[0], semi[1], semi[2], opaque[0],
            opaque[1], opaque[2]);
    blended = near(semi[0], 137, 8) && near(semi[1], 127, 8) && near(semi[2], 127, 8);
    failed = 0;
    failed |= check(blended, "semi cmd 0x2a blends 20,0,0 over white");
    failed |= check(near(opaque[0], 20, 8) && opaque[1] <= 8 && opaque[2] <= 8, "opaque quad stays 20,0,0");
    gpu_debug_reset_clear();
    host_clear();
    return failed;
}

static int check_nclip(void) {
    MATRIX saved;
    GteSettings settings;
    SVECTOR a, b, c;
    long front;
    long back;
    int failed;

    memset(&saved, 0, sizeof(saved));
    gte_ReadRotMatrix(&saved);
    gte_debug_settings(&settings);
    gte_set_rotation_identity();
    gte_set_translation(0, 0, 0);
    SetGeomOffset(160, 120);
    SetGeomScreen(1000);

    a.vx = 0;
    a.vy = 0;
    a.vz = 1000;
    a.pad = 0;
    b.vx = 100;
    b.vy = 0;
    b.vz = 1000;
    b.pad = 0;
    c.vx = 0;
    c.vy = 100;
    c.vz = 1000;
    c.pad = 0;
    gte_ldv3(&a, &b, &c);
    gte_rtpt();
    gte_nclip();
    gte_stopz(&front);

    gte_ldv3(&a, &c, &b);
    gte_rtpt();
    gte_nclip();
    gte_stopz(&back);
    SDL_Log("scenery check nclip front %ld back %ld", front, back);

    failed = 0;
    failed |= check(front > 0, "front face passes nclip");
    failed |= check(back < 0, "back face fails nclip");

    gte_SetRotMatrix(&saved);
    gte_SetTransMatrix(&saved);
    SetGeomOffset(settings.off_x, settings.off_y);
    SetGeomScreen(settings.screen_h);
    return failed;
}

int scenery_self_check(void) {
    int failed;

    failed = 0;
    failed |= check_blend();
    failed |= check_nclip();
    return failed == 0 ? 0 : 1;
}
