/* libgs: the game uses its work base, offsets, and screen size globals. */

#include <libgs.h>
#include <object_trace.h>

MATRIX GsIDMATRIX = {{{ONE, 0, 0}, {0, ONE, 0}, {0, 0, ONE}}, {0, 0, 0}};
u_short HWD0 = 320;
u_short VWD0 = 240;

static PACKET *gs_work_base;

void GsInitGraph(unsigned short x, unsigned short y, unsigned short intmode,
                 unsigned short dith, unsigned short varmmode) {
    (void)intmode;
    (void)dith;
    (void)varmmode;
    HWD0 = x;
    VWD0 = y;
}

void GsDefDispBuff(unsigned short x0, unsigned short y0, unsigned short x1, unsigned short y1) {
    (void)x0;
    (void)y0;
    (void)x1;
    (void)y1;
}

void GsSetOffset(long x, long y) {
    (void)x;
    (void)y;
}

void GsSetDrawBuffOffset(void) {
}

void GsInit3D(void) {
}

void GsSetWorkBase(PACKET *base) {
    gs_work_base = base;
}

PACKET *GsGetWorkBase(void) {
    return gs_work_base;
}

void GsSetProjection(long h) {
    (void)h;
}

static GsF_LIGHT flat_light[3];
static int flat_light_set[3];
static int ambient_rgb[3];
static int ambient_set;
static int light_mode;
static int light_mode_set;

int GsSetFlatLight(int id, GsF_LIGHT *lt) {
    if (id >= 0 && id < 3 && lt != NULL) {
        flat_light[id] = *lt;
        flat_light_set[id] = 1;
    }
    return 0;
}

void GsSetLightMode(int mode) {
    light_mode = mode;
    light_mode_set = 1;
}

void GsSetAmbient(long r, long g, long b) {
    ambient_rgb[0] = (int)r;
    ambient_rgb[1] = (int)g;
    ambient_rgb[2] = (int)b;
    ambient_set = 1;
}

int gs_debug_flat_light(int id, int *r, int *g, int *b) {
    if (id < 0 || id > 2 || !flat_light_set[id]) {
        return 0;
    }
    *r = flat_light[id].r;
    *g = flat_light[id].g;
    *b = flat_light[id].b;
    return 1;
}

int gs_debug_ambient(int *r, int *g, int *b) {
    if (!ambient_set) {
        return 0;
    }
    *r = ambient_rgb[0];
    *g = ambient_rgb[1];
    *b = ambient_rgb[2];
    return 1;
}

int gs_debug_flat_dir(int id, int *vx, int *vy, int *vz) {
    if (id < 0 || id > 2 || !flat_light_set[id]) {
        return 0;
    }
    *vx = (int)flat_light[id].vx;
    *vy = (int)flat_light[id].vy;
    *vz = (int)flat_light[id].vz;
    return 1;
}

int gs_debug_light_mode(int *mode) {
    if (!light_mode_set) {
        return 0;
    }
    *mode = light_mode;
    return 1;
}
