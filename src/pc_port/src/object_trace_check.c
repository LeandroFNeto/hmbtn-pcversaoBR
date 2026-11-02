/* Object-trace checks, run at startup when HMBTN_SELFTEST=1. */

#include <gte.h>
#include <host.h>
#include <inline_c.h>
#include <libgpu.h>
#include <libgs.h>
#include <libgte.h>
#include <object_trace.h>
#include <vram.h>

#include <SDL.h>

#include <string.h>

int GsSetFlatLight(int id, GsF_LIGHT *lt);

static int check(int ok, const char *what) {
    SDL_Log("object trace check %s: %s", ok ? "ok" : "FAILED", what);
    return ok ? 0 : 1;
}

static int has(const char *text) {
    return strstr(object_trace_log(), text) != NULL;
}

static void camera(void) {
    gte_set_rotation_identity();
    gte_set_translation(0, 0, 0);
    SetGeomOffset(160, 120);
    SetGeomScreen(1000);
}

static void project_at(int sx, int sy) {
    SVECTOR v;

    v.vx = (short)(sx - 160);
    v.vy = (short)(sy - 120);
    v.vz = 1000;
    v.pad = 0;
    gte_ldv0(&v);
    gte_rtps();
}

static void one_prim(const SVECTOR *a, const SVECTOR *b, const SVECTOR *c) {
    long flag;
    long mac;

    gte_ldv3(a, b, c);
    gte_rtpt();
    gte_stflg(&flag);
    if (flag < 0) {
        return;
    }
    gte_nclip();
    gte_stopz(&mac);
}

static SVECTOR sv(int x, int y) {
    SVECTOR v;

    v.vx = (short)x;
    v.vy = (short)y;
    v.vz = 1000;
    v.pad = 0;
    return v;
}

static int check_lights(void) {
    GsF_LIGHT light;
    POLY_F4 quad;
    unsigned char rgb[3];
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
    int ret;
    int stored;
    int failed;

    memset(&light, 0, sizeof(light));
    light.vx = 100;
    light.vy = 100;
    light.vz = 100;
    light.r = 208;
    light.g = 208;
    light.b = 208;
    ret = GsSetFlatLight(0, &light);
    light.r = 0;
    light.g = 0;
    light.b = 0;
    GsSetFlatLight(1, &light);
    GsSetFlatLight(2, &light);
    GsSetAmbient(2048, 2048, 2048);
    GsSetLightMode(0);
    stored = ret == 0 && gs_debug_flat_dir(0, &vx, &vy, &vz) && vx == 100 && vy == 100 && vz == 100 &&
             gs_debug_flat_light(0, &r, &g, &b) && r == 208 && g == 208 && b == 208 &&
             gs_debug_flat_light(1, &r, &g, &b) && r == 0 && g == 0 && b == 0 &&
             gs_debug_flat_light(2, &r, &g, &b) && r == 0 && g == 0 && b == 0 &&
             gs_debug_ambient(&ar, &ag, &ab) && ar == 2048 && ag == 2048 && ab == 2048 &&
             gs_debug_light_mode(&mode) && mode == 0;
    failed = check(stored,
                   "GsSetFlatLight(0) as direction (100, 100, 100) and color (208, 208, 208), lights 1 and 2 as color (0, 0, 0), ambient as (2048, 2048, 2048), mode as 0, and the call returning 0");

    host_clear();
    memset(&quad, 0, sizeof(quad));
    setPolyF4(&quad);
    setRGB0(&quad, 0, 255, 0);
    setXY4(&quad, 200, 40, 216, 40, 200, 56, 216, 56);
    termPrim(&quad);
    DrawOTag((u_long *)&quad);
    gpu_debug_read_pixel(204, 44, rgb);
    failed |= check(rgb[0] <= 8 && rgb[1] >= 247 && rgb[2] <= 8,
                    "a drawn polygon keeps its packet color when the flat light differs");
    host_clear();
    return failed;
}

static int check_walk(void) {
    static POLY_F3 poly;
    static TILE tile;
    static struct {
        u_long tag;
        u_char r0, g0, b0, code;
        short x0, y0;
        short x1, y1;
        u_long extra;
    } line;
    static struct {
        u_long tag;
        u_char r0, g0, b0, code;
    } mask;
    int failed;

    memset(&poly, 0, sizeof(poly));
    setPolyF3(&poly);
    setRGB0(&poly, 255, 0, 0);
    setXY3(&poly, 0, 0, 4, 0, 0, 4);
    memset(&tile, 0, sizeof(tile));
    setTile(&tile);
    setRGB0(&tile, 0, 0, 255);
    setXY0(&tile, 8, 8);
    setWH(&tile, 2, 2);
    memset(&line, 0, sizeof(line));
    setLineF2(&line);
    setlen(&line, 4);
    setXY2(&line, 0, 2, 4, 2);
    line.extra = 0x28000000u;
    memset(&mask, 0, sizeof(mask));
    setlen(&mask, 1);
    setcode(&mask, 0xE6);
    catPrim(&poly, &tile);
    catPrim(&tile, &line);
    catPrim(&line, &mask);
    termPrim(&mask);

    object_trace_debug_begin();
    DrawOTag((u_long *)&poly);
    object_trace_debug_end();
    failed = check(has("poly 1 rect 1 line 1 line_cut 1 mask 1"),
                   "the walk reports poly 1 rect 1 line 1 line_cut 1 mask 1");
    host_clear();
    return failed;
}

static int check_center(void) {
    POLY_F4 black;
    DR_TPAGE tpage;
    SPRT sprt;
    uint16_t zeros[16 * 4];
    int failed;

    object_trace_debug_begin();
    object_trace_debug_end();
    failed = check(has("trace object: center uncovered"), "an empty list logs center uncovered");

    host_clear();
    memset(&black, 0, sizeof(black));
    setPolyF4(&black);
    setRGB0(&black, 0, 0, 0);
    setXY4(&black, 140, 100, 180, 100, 140, 140, 180, 140);
    termPrim(&black);
    object_trace_debug_begin();
    DrawOTag((u_long *)&black);
    object_trace_debug_end();
    failed |= check(has("rgb 0,0,0"), "an untextured primitive covering the center logs rgb 0,0,0");

    memset(zeros, 0, sizeof(zeros));
    vram_load(640, 256, 16, 4, zeros);
    memset(&tpage, 0, sizeof(tpage));
    setDrawTPage(&tpage, 0, 0, getTPage(2, 0, 640, 256));
    termPrim(&tpage);
    DrawOTag((u_long *)&tpage);
    memset(&sprt, 0, sizeof(sprt));
    setSprt(&sprt);
    setRGB0(&sprt, 128, 128, 128);
    setXY0(&sprt, 158, 118);
    setUV0(&sprt, 0, 0);
    setWH(&sprt, 4, 4);
    termPrim(&sprt);
    object_trace_debug_begin();
    DrawOTag((u_long *)&sprt);
    object_trace_debug_end();
    failed |= check(has("cmd 0x64") && has("word 0x0000"),
                    "a textured primitive covering the center logs word 0x0000");
    host_clear();
    return failed;
}

static int check_groups(void) {
    SVECTOR a;
    SVECTOR b;
    SVECTOR c;
    SVECTOR s0;
    SVECTOR s1;
    SVECTOR s2;
    int failed;

    camera();
    a = sv(-50, -50);
    b = sv(50, -50);
    c = sv(0, 50);

    object_trace_debug_begin();
    PushMatrix();
    project_at(160, 120);
    PushMatrix();
    one_prim(&a, &c, &b);
    PopMatrix();
    PopMatrix();
    object_trace_debug_end();
    failed = check(has("group x 160 y 120") && has("nclip_fail 1") && has("nclip_pass 0"),
                   "a central group whose backfaces all fail logs the point, a fail, and zero passes");

    object_trace_debug_begin();
    PushMatrix();
    project_at(160, 120);
    PopMatrix();
    object_trace_debug_end();
    failed |= check(has("rtpt 0") && has("never entered its primitive pass"),
                    "a projection with no rtpt logs that it did not enter");

    object_trace_debug_begin();
    PushMatrix();
    project_at(0, 0);
    PopMatrix();
    PushMatrix();
    project_at(400, 120);
    PopMatrix();
    object_trace_debug_end();
    failed |= check(has("group x 0 y 0") && !has("group x 400"),
                    "when every projection is outside the central half, the log reports the nearest group");

    s0 = sv(-70, -30);
    s1 = sv(-40, -30);
    s2 = sv(-70, -10);
    object_trace_debug_begin();
    PushMatrix();
    project_at(100, 100);
    PushMatrix();
    one_prim(&s0, &s1, &s2);
    PopMatrix();
    PopMatrix();
    {
        LINE_F2 line;

        memset(&line, 0, sizeof(line));
        setLineF2(&line);
        setXY2(&line, 0, 0, 2, 2);
        termPrim(&line);
        DrawOTag((u_long *)&line);
    }
    object_trace_debug_end();
    failed |= check(has("skipped line or mask command"), "a passing group that misses the center names a skipped line");

    return failed;
}

static void draw_f3(int r, int g, int b) {
    POLY_F3 poly;

    memset(&poly, 0, sizeof(poly));
    setPolyF3(&poly);
    setRGB0(&poly, r, g, b);
    setXY3(&poly, 110, 70, 210, 70, 160, 170);
    termPrim(&poly);
    DrawOTag((u_long *)&poly);
}

static void passing_entry(void) {
    SVECTOR a;
    SVECTOR b;
    SVECTOR c;

    a = sv(-50, -50);
    b = sv(50, -50);
    c = sv(0, 50);
    PushMatrix();
    project_at(160, 120);
    PushMatrix();
    one_prim(&a, &b, &c);
    PopMatrix();
    PopMatrix();
}

static int check_outcomes(void) {
    POLY_F4 ground;
    int failed;

    camera();

    object_trace_debug_begin();
    passing_entry();
    draw_f3(0, 0, 0);
    object_trace_debug_end();
    failed = check(has("outcome black emitted primitive at 160,120"), "a black primitive of the group names black emitted primitive");

    object_trace_debug_begin();
    PushMatrix();
    project_at(160, 120);
    PushMatrix();
    {
        SVECTOR a;
        SVECTOR b;
        SVECTOR c;

        a = sv(-50, -50);
        b = sv(50, -50);
        c = sv(0, 50);
        one_prim(&a, &c, &b);
    }
    PopMatrix();
    PopMatrix();
    memset(&ground, 0, sizeof(ground));
    setPolyF4(&ground);
    setRGB0(&ground, 0, 255, 0);
    setXY4(&ground, 140, 100, 180, 100, 140, 140, 180, 140);
    termPrim(&ground);
    DrawOTag((u_long *)&ground);
    object_trace_debug_end();
    failed |= check(has("outcome geometry discard at 160,120") && !has("black emitted primitive"),
                    "a ground primitive at the center leaves the group's geometry discard unchanged");

    object_trace_debug_begin();
    passing_entry();
    draw_f3(40, 80, 120);
    object_trace_debug_end();
    failed |= check(has("outcome none at 160,120"), "a colored primitive of the group names none");

    host_clear();
    return failed;
}

static int check_arm(void) {
    POLY_F3 poly;
    const char *log;
    int failed;
    int n;

    object_trace_debug_reset();
    object_trace_debug_set_enabled(0);
    object_trace_present(1);
    failed = check(strstr(object_trace_log(), "trace object:") == NULL,
                   "F8 with the dump switch unset writes no trace object lines");

    object_trace_debug_reset();
    object_trace_debug_set_enabled(1);
    object_trace_present(1);
    memset(&poly, 0, sizeof(poly));
    setPolyF3(&poly);
    setRGB0(&poly, 1, 2, 3);
    setXY3(&poly, 1, 1, 3, 1, 1, 3);
    termPrim(&poly);
    DrawOTag((u_long *)&poly);
    object_trace_present(0);
    log = object_trace_log();
    failed |= check(strstr(log, "trace object: walk") != NULL && strstr(log, "poly 1") != NULL,
                    "one arm writes one frame");
    n = (int)strlen(log);
    object_trace_present(1);
    failed |= check((int)strlen(object_trace_log()) == n, "the following frame writes none");
    failed |= check(strstr(object_trace_log(), "trace object:") != NULL, "the arm writes its lines with the black-region trace left alone");
    object_trace_debug_reset();
    host_clear();
    return failed;
}

int object_trace_self_check(void) {
    int failed;

    failed = check_lights();
    failed |= check_walk();
    failed |= check_center();
    failed |= check_groups();
    failed |= check_outcomes();
    failed |= check_arm();
    object_trace_debug_reset();
    return failed;
}
