/* libgs: the game uses its work base, offsets, and screen size globals. */

#include <libgs.h>

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

int GsSetFlatLight(int id, GsF_LIGHT *lt) {
    (void)id;
    (void)lt;
    return 0;
}

void GsSetLightMode(int mode) {
    (void)mode;
}

void GsSetAmbient(long r, long g, long b) {
    (void)r;
    (void)g;
    (void)b;
}
