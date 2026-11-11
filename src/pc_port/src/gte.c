#include <gte.h>
#include <gtemac.h>
#include <object_trace.h>
#include <objects_dump.h>

#include <stdint.h>
#include <string.h>

/* 1.3.12 rotation (4096 == 1), translation, pixel offset, screen distance. */
static int rot[3][3];
static int tr[3];
static int off_x;
static int off_y;
static int screen_h;
static int behind;

void gte_set_rotation_identity(void) {
    int r;
    int c;

    for (r = 0; r < 3; r++) {
        for (c = 0; c < 3; c++) {
            rot[r][c] = 0;
        }
        rot[r][r] = 4096;
    }
}

void gte_set_translation(int tx, int ty, int tz) {
    tr[0] = tx;
    tr[1] = ty;
    tr[2] = tz;
}

void SetGeomOffset(long ox, long oy) {
    off_x = (int)ox;
    off_y = (int)oy;
}

void SetGeomScreen(long h) {
    screen_h = (int)h;
}

int gte_project(int vx, int vy, int vz, short *sx, short *sy) {
    int x;
    int y;
    int z;

    x = ((rot[0][0] * vx + rot[0][1] * vy + rot[0][2] * vz) >> 12) + tr[0];
    y = ((rot[1][0] * vx + rot[1][1] * vy + rot[1][2] * vz) >> 12) + tr[1];
    z = ((rot[2][0] * vx + rot[2][1] * vy + rot[2][2] * vz) >> 12) + tr[2];
    if (z <= 0) {
        behind = 1;
        return -1;
    }
    behind = 0;
    if (sx != 0) {
        *sx = (short)(off_x + x * screen_h / z);
    }
    if (sy != 0) {
        *sy = (short)(off_y + y * screen_h / z);
    }
    return 0;
}

int gte_startup_check(void) {
    short sx;
    short sy;

    gte_set_rotation_identity();
    gte_set_translation(0, 0, 0);
    SetGeomOffset(160, 120);
    SetGeomScreen(1000);

    if (gte_project(0, 0, 1000, &sx, &sy) != 0) {
        return 1;
    }
    if (sx != 160 || sy != 120) {
        return 1;
    }

    if (gte_project(1000, 0, 1000, &sx, &sy) != 0) {
        return 1;
    }
    if (sx <= 160 || sy != 120) {
        return 1;
    }

    if (gte_project(0, 0, 0, &sx, &sy) == 0) {
        return 1;
    }
    if (gte_project(0, 0, -10, &sx, &sy) == 0) {
        return 1;
    }
    return 0;
}

/* Coprocessor registers the game reads back: input vectors, the screen XY and Z
 * FIFOs, MAC0 (NCLIP), OTZ, IR1-3, MAC1-3, and FLAG. */
static int vin[3][3];
static int sxy[3][2];
static int sz[4];
static int mac0;
static int otz;
static int ir[3];
static int mac[3];
static unsigned int flag;

#define GTE_FLAG_ERROR 0x80000000u
#define GTE_ZSF3 0x155
#define GTE_ZSF4 0x100

/* FLAG bits (psx-spx, "GTE Saturation"): MAC1-3 past 43 bits, IR1-3 saturated, and the
 * error bit, set when any of bits 30..23 or 18..13 is. */
#define GTE_FLAG_MAC_POS(i) (1u << (30 - (i)))
#define GTE_FLAG_MAC_NEG(i) (1u << (27 - (i)))
#define GTE_FLAG_IR(i) (1u << (24 - (i)))
#define GTE_FLAG_ERROR_BITS 0x7F87E000u

/* Average-Z factors, far color (RFC, GFC, BFC), and depth cueing (DQA, DQB). Only
 * InitGeom, SetFarColor, and SetFogNear write the last two groups. The back color
 * and the light and color matrices are stored for the game's setters; no command
 * the game issues reads them back into a polygon. */
static int zsf3 = GTE_ZSF3;
static int zsf4 = GTE_ZSF4;
static int far_color[3];
static int back_color[3];
static short color_matrix[3][3];
static short light_matrix[3][3];
static int dqa;
static int dqb;

static void copy_matrix9(short dst[3][3], const void *m) {
    const MATRIX *mat;
    int r;
    int c;

    mat = (const MATRIX *)m;
    for (r = 0; r < 3; r++) {
        for (c = 0; c < 3; c++) {
            dst[r][c] = mat->m[r][c];
        }
    }
}

static int clamp_s16(int v) {
    if (v < -32768) {
        return -32768;
    }
    if (v > 32767) {
        return 32767;
    }
    return v;
}

static void load_svector(int dst[3], const void *v) {
    const short *s;

    s = (const short *)v;
    dst[0] = s[0];
    dst[1] = s[1];
    dst[2] = s[2];
}

static void push_screen(int index) {
    int x;
    int y;
    int z;
    int i;

    x = ((rot[0][0] * vin[index][0] + rot[0][1] * vin[index][1] + rot[0][2] * vin[index][2]) >> 12) + tr[0];
    y = ((rot[1][0] * vin[index][0] + rot[1][1] * vin[index][1] + rot[1][2] * vin[index][2]) >> 12) + tr[1];
    z = ((rot[2][0] * vin[index][0] + rot[2][1] * vin[index][1] + rot[2][2] * vin[index][2]) >> 12) + tr[2];
    ir[0] = clamp_s16(x);
    ir[1] = clamp_s16(y);
    ir[2] = clamp_s16(z);
    for (i = 0; i < 2; i++) {
        sxy[i][0] = sxy[i + 1][0];
        sxy[i][1] = sxy[i + 1][1];
    }
    sz[0] = sz[1];
    sz[1] = sz[2];
    sz[2] = sz[3];
    if (z <= 0) {
        flag |= GTE_FLAG_ERROR;
        sz[3] = 0;
        sxy[2][0] = off_x;
        sxy[2][1] = off_y;
        return;
    }
    sz[3] = z > 0xFFFF ? 0xFFFF : z;
    sxy[2][0] = clamp_s16(off_x + x * screen_h / z);
    sxy[2][1] = clamp_s16(off_y + y * screen_h / z);
}

static void store_word(void *p, int value) {
    memcpy(p, &value, sizeof(value));
}

static unsigned int pack_sxy(int index) {
    return ((unsigned int)(unsigned short)sxy[index][1] << 16) | (unsigned short)sxy[index][0];
}

void gte_h_ldv0(const void *v) {
    load_svector(vin[0], v);
}

void gte_h_ldv3(const void *v0, const void *v1, const void *v2) {
    load_svector(vin[0], v0);
    load_svector(vin[1], v1);
    load_svector(vin[2], v2);
}

void gte_h_ldlv0(const void *v) {
    const long *l;

    l = (const long *)v;
    vin[0][0] = (short)l[0];
    vin[0][1] = (short)l[1];
    vin[0][2] = (short)l[2];
}

void gte_h_ldlvl(const void *v) {
    const long *l;

    l = (const long *)v;
    ir[0] = (int)l[0];
    ir[1] = (int)l[1];
    ir[2] = (int)l[2];
}

void gte_h_ldclmv(const void *m) {
    const short *s;

    s = (const short *)m;
    ir[0] = s[0];
    ir[1] = s[3];
    ir[2] = s[6];
}

void gte_h_rtps(void) {
    flag = 0;
    push_screen(0);
    object_trace_rtps(sxy[2][0], sxy[2][1]);
}

void gte_h_rtpt(void) {
    flag = 0;
    push_screen(0);
    push_screen(1);
    push_screen(2);
    object_trace_rtpt();
}

static void mul_rot(const int v[3]) {
    int i;

    for (i = 0; i < 3; i++) {
        mac[i] = (rot[i][0] * v[0] + rot[i][1] * v[1] + rot[i][2] * v[2]) >> 12;
        ir[i] = clamp_s16(mac[i]);
    }
}

/* The game's gte.h encodes RTV0 as 0x4A480012: rotation * V0 + TR. */
void gte_h_rtv0(void) {
    int i;

    for (i = 0; i < 3; i++) {
        mac[i] = (int)(((int64_t)tr[i] * 4096 + (int64_t)rot[i][0] * vin[0][0] +
                        (int64_t)rot[i][1] * vin[0][1] + (int64_t)rot[i][2] * vin[0][2]) >> 12);
        ir[i] = clamp_s16(mac[i]);
    }
}

void gte_h_rtir(void) {
    int v[3];

    v[0] = ir[0];
    v[1] = ir[1];
    v[2] = ir[2];
    mul_rot(v);
}

void gte_h_nclip(void) {
    mac0 = sxy[0][0] * sxy[1][1] + sxy[1][0] * sxy[2][1] + sxy[2][0] * sxy[0][1] -
           sxy[0][0] * sxy[2][1] - sxy[1][0] * sxy[0][1] - sxy[2][0] * sxy[1][1];
    object_trace_nclip();
    objects_dump_note_nclip(mac0);
}

void gte_h_avsz3(void) {
    mac0 = zsf3 * (sz[1] + sz[2] + sz[3]);
    otz = mac0 >> 12;
}

void gte_h_avsz4(void) {
    mac0 = zsf4 * (sz[0] + sz[1] + sz[2] + sz[3]);
    otz = mac0 >> 12;
}

void gte_h_sqr0(void) {
    int i;

    for (i = 0; i < 3; i++) {
        mac[i] = ir[i] * ir[i];
        ir[i] = mac[i] > 0x7FFF ? 0x7FFF : mac[i];
    }
}

void gte_h_op12(void) {
    int d0;
    int d1;
    int d2;

    d0 = rot[0][0];
    d1 = rot[1][1];
    d2 = rot[2][2];
    mac[0] = (d1 * ir[2] - d2 * ir[1]) >> 12;
    mac[1] = (d2 * ir[0] - d0 * ir[2]) >> 12;
    mac[2] = (d0 * ir[1] - d1 * ir[0]) >> 12;
    ir[0] = clamp_s16(mac[0]);
    ir[1] = clamp_s16(mac[1]);
    ir[2] = clamp_s16(mac[2]);
}

void gte_h_stsxy(void *p) {
    store_word(p, (int)pack_sxy(2));
}

void gte_h_stsxy3(void *p, int off0, int off1, int off2) {
    unsigned char *b;

    b = (unsigned char *)p;
    store_word(b + off0, (int)pack_sxy(0));
    store_word(b + off1, (int)pack_sxy(1));
    store_word(b + off2, (int)pack_sxy(2));
}

void gte_h_stflg(void *p) {
    store_word(p, (int)flag);
    object_trace_stflg((int)flag);
    objects_dump_note_flag((int)flag);
}

void gte_h_stopz(void *p) {
    store_word(p, mac0);
    object_trace_stopz(mac0, sxy[0][0], sxy[0][1], sxy[1][0], sxy[1][1], sxy[2][0], sxy[2][1]);
}

void gte_h_stotz(void *p) {
    store_word(p, otz);
}

void gte_h_stszotz(void *p) {
    store_word(p, sz[3] >> 2);
}

void gte_h_stlvl(void *p) {
    long *l;

    l = (long *)p;
    l[0] = ir[0];
    l[1] = ir[1];
    l[2] = ir[2];
}

void gte_h_stlvnl(void *p) {
    long *l;

    l = (long *)p;
    l[0] = mac[0];
    l[1] = mac[1];
    l[2] = mac[2];
}

void gte_h_stclmv(void *m) {
    short *s;

    s = (short *)m;
    s[0] = (short)ir[0];
    s[3] = (short)ir[1];
    s[6] = (short)ir[2];
}

void gte_h_SetRotMatrix(const void *m) {
    const MATRIX *mat;
    int r;
    int c;

    mat = (const MATRIX *)m;
    for (r = 0; r < 3; r++) {
        for (c = 0; c < 3; c++) {
            rot[r][c] = mat->m[r][c];
        }
    }
}

void gte_h_SetTransMatrix(const void *m) {
    const MATRIX *mat;

    mat = (const MATRIX *)m;
    tr[0] = (int)mat->t[0];
    tr[1] = (int)mat->t[1];
    tr[2] = (int)mat->t[2];
}

void gte_h_ReadRotMatrix(void *m) {
    MATRIX *mat;
    int r;
    int c;

    mat = (MATRIX *)m;
    for (r = 0; r < 3; r++) {
        for (c = 0; c < 3; c++) {
            mat->m[r][c] = (short)rot[r][c];
        }
    }
    mat->t[0] = tr[0];
    mat->t[1] = tr[1];
    mat->t[2] = tr[2];
}

void gte_h_SetBackColor(int r, int g, int b) {
    back_color[0] = r;
    back_color[1] = g;
    back_color[2] = b;
}

void gte_h_SetColorMatrix(const void *m) {
    copy_matrix9(color_matrix, m);
}

void gte_h_SetLightMatrix(const void *m) {
    copy_matrix9(light_matrix, m);
}

void gte_h_MulMatrix0(const void *m0, const void *m1, void *m2) {
    const MATRIX *a;
    const MATRIX *b;
    MATRIX out;
    int r;
    int c;

    a = (const MATRIX *)m0;
    b = (const MATRIX *)m1;
    for (r = 0; r < 3; r++) {
        for (c = 0; c < 3; c++) {
            out.m[r][c] = (short)((a->m[r][0] * b->m[0][c] + a->m[r][1] * b->m[1][c] +
                                   a->m[r][2] * b->m[2][c]) >> 12);
        }
    }
    for (r = 0; r < 3; r++) {
        for (c = 0; c < 3; c++) {
            ((MATRIX *)m2)->m[r][c] = out.m[r][c];
        }
    }
}

void gte_h_OuterProduct12(const void *v0, const void *v1, void *v2) {
    const long *a;
    const long *b;
    long *o;
    long x;
    long y;
    long z;

    a = (const long *)v0;
    b = (const long *)v1;
    o = (long *)v2;
    x = (a[1] * b[2] - a[2] * b[1]) >> 12;
    y = (a[2] * b[0] - a[0] * b[2]) >> 12;
    z = (a[0] * b[1] - a[1] * b[0]) >> 12;
    o[0] = x;
    o[1] = y;
    o[2] = z;
}

/* MAC i from a 44-bit sum: overflow flags on the sum, then the optional >> 12. MAC keeps
 * the low 32 bits; IR i saturates the shifted value, to 0..0x7FFF when lm is set. */
static void store_mac_ir(int i, int64_t sum, int sf, int lm) {
    int64_t shifted;
    int64_t low;

    if (sum > INT64_C(0x7FFFFFFFFFF)) {
        flag |= GTE_FLAG_MAC_POS(i);
    } else if (sum < -INT64_C(0x80000000000)) {
        flag |= GTE_FLAG_MAC_NEG(i);
    }
    shifted = sf ? sum >> 12 : sum;
    mac[i] = (int)(int32_t)(uint32_t)shifted;
    low = lm ? 0 : -0x8000;
    if (shifted < low) {
        ir[i] = (int)low;
        flag |= GTE_FLAG_IR(i);
    } else if (shifted > 0x7FFF) {
        ir[i] = 0x7FFF;
        flag |= GTE_FLAG_IR(i);
    } else {
        ir[i] = (int)shifted;
    }
}

static void finish(const int64_t sum[3], int sf, int lm, int out[3]) {
    int i;

    for (i = 0; i < 3; i++) {
        store_mac_ir(i, sum[i], sf, lm);
        out[i] = mac[i];
    }
    if (flag & GTE_FLAG_ERROR_BITS) {
        flag |= GTE_FLAG_ERROR;
    }
}

void gte_mvmva_rot(const int v[3], int translate, int sf, int lm, int out[3]) {
    int64_t sum[3];
    int i;

    flag = 0;
    for (i = 0; i < 3; i++) {
        sum[i] = translate ? (int64_t)tr[i] * 4096 : 0;
        sum[i] += (int64_t)rot[i][0] * (short)v[0] + (int64_t)rot[i][1] * (short)v[1] +
                  (int64_t)rot[i][2] * (short)v[2];
    }
    finish(sum, sf, lm, out);
}

void gte_op(const int d[3], const int v[3], int sf, int lm, int out[3]) {
    int64_t sum[3];

    flag = 0;
    sum[0] = (int64_t)(short)d[1] * (short)v[2] - (int64_t)(short)d[2] * (short)v[1];
    sum[1] = (int64_t)(short)d[2] * (short)v[0] - (int64_t)(short)d[0] * (short)v[2];
    sum[2] = (int64_t)(short)d[0] * (short)v[1] - (int64_t)(short)d[1] * (short)v[0];
    finish(sum, sf, lm, out);
}

void gte_sqr(const int v[3], int sf, int lm, int out[3]) {
    int64_t sum[3];
    int i;

    flag = 0;
    for (i = 0; i < 3; i++) {
        sum[i] = (int64_t)(short)v[i] * (short)v[i];
    }
    finish(sum, sf, lm, out);
}

void gte_gpf(int ir0, const int v[3], int sf, int lm, int out[3]) {
    int64_t sum[3];
    int i;

    flag = 0;
    for (i = 0; i < 3; i++) {
        sum[i] = (int64_t)(short)ir0 * (short)v[i];
    }
    finish(sum, sf, lm, out);
}

int gte_lzc(int value) {
    uint32_t bits;
    int count;

    bits = value < 0 ? ~(uint32_t)value : (uint32_t)value;
    count = 0;
    while (count < 32 && !(bits & 0x80000000u)) {
        bits <<= 1;
        count++;
    }
    return count;
}

unsigned int gte_flag_value(void) {
    return flag;
}

void gte_init_geom(void) {
    zsf3 = 341;
    zsf4 = 256;
    screen_h = 1000;
    off_x = 0;
    off_y = 0;
    dqa = -4194;
    dqb = 0x1400000;
}

void gte_set_far_color(int r, int g, int b) {
    far_color[0] = r;
    far_color[1] = g;
    far_color[2] = b;
}

void gte_set_depth_cue(int a, int b) {
    dqa = a;
    dqb = b;
}

void gte_debug_settings(GteSettings *s) {
    s->screen_h = screen_h;
    s->off_x = off_x;
    s->off_y = off_y;
    s->dqa = dqa;
    s->dqb = dqb;
    s->zsf3 = zsf3;
    s->zsf4 = zsf4;
    s->far_color[0] = far_color[0];
    s->far_color[1] = far_color[1];
    s->far_color[2] = far_color[2];
    s->back_color[0] = back_color[0];
    s->back_color[1] = back_color[1];
    s->back_color[2] = back_color[2];
    memcpy(s->color_matrix, color_matrix, sizeof(s->color_matrix));
    memcpy(s->light_matrix, light_matrix, sizeof(s->light_matrix));
}
