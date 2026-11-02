/* libgte's CPU-side math: the functions the game calls, with the results of the PSY-Q 4.7
 * LIBGTE objects it links. Products are 32-bit and shifts arithmetic, as the R3000 does
 * them. The lookup tables follow closed formulas that reproduce the game's own tables
 * entry for entry, except the arctangent table, which is read from the game's executable
 * on the disc. */

#include <cd_drive.h>
#include <gte.h>
#include <libgte.h>
#include <object_trace.h>

#include <SDL.h>

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define EXE_NAME "\\SLUS_011.15;1"
#define RATAN_OFFSET 0x4D0C4u
#define MATRIX_STACK_DEPTH 20

static short sin_tbl[1025];         /* a quarter turn: round(sin(i * pi / 2048) * 4096) */
static short cossin_tbl[4096 * 2];  /* rcossin_tbl: {sin, cos} per angle, same rounding */
static short sqrt_tbl[192];         /* SQRT: floor(sqrt(64 + i) * 512) */
static short inv_sqrt_tbl[192];     /* VectorNormal's: floor(32768 / sqrt(64 + i)) */
static short ratan_tbl[1025];       /* from SLUS_011.15 at RATAN_OFFSET */
static int tables_ready;
static int ratan_state;             /* 0 not tried, 1 loaded, -1 unavailable */

static MATRIX matrix_stack[MATRIX_STACK_DEPTH];
static int matrix_depth;

static short round_table(double x) {
    return (short)floor(x + 0.5);
}

static void tables_init(void) {
    const double step = 3.14159265358979323846 / 2048.0;
    int i;

    if (tables_ready) {
        return;
    }
    for (i = 0; i < 1025; i++) {
        sin_tbl[i] = round_table(sin(i * step) * 4096.0);
    }
    for (i = 0; i < 4096; i++) {
        cossin_tbl[i * 2] = round_table(sin(i * step) * 4096.0);
        cossin_tbl[i * 2 + 1] = round_table(cos(i * step) * 4096.0);
    }
    for (i = 0; i < 192; i++) {
        sqrt_tbl[i] = (short)floor(sqrt(64.0 + i) * 512.0);
        inv_sqrt_tbl[i] = (short)floor(32768.0 / sqrt(64.0 + i));
    }
    tables_ready = 1;
}

int gte_exe_read(uint32_t offset, void *dst, uint32_t bytes) {
    uint32_t lba;
    uint32_t size;
    uint32_t first;
    uint32_t sectors;
    unsigned char *buf;
    int result;

    if (!cd_drive_is_open() || cd_drive_find(EXE_NAME, &lba, &size) != 0 || offset + bytes > size) {
        return -1;
    }
    first = offset / CD_DRIVE_SECTOR;
    sectors = (offset % CD_DRIVE_SECTOR + bytes + CD_DRIVE_SECTOR - 1) / CD_DRIVE_SECTOR;
    buf = (unsigned char *)malloc(sectors * CD_DRIVE_SECTOR);
    if (buf == NULL) {
        return -1;
    }
    result = cd_drive_read(lba + first, sectors, buf);
    if (result == 0) {
        memcpy(dst, buf + offset % CD_DRIVE_SECTOR, bytes);
    }
    free(buf);
    return result;
}

int gte_ratan_table_load(void) {
    int i;
    int ok;

    if (ratan_state != 0) {
        return ratan_state > 0 ? 0 : -1;
    }
    ratan_state = -1;
    if (gte_exe_read(RATAN_OFFSET, ratan_tbl, sizeof(ratan_tbl)) != 0) {
        SDL_Log("gte: cannot read the arctangent table from %s; ratan2 returns 0", EXE_NAME + 1);
        return -1;
    }
    ok = ratan_tbl[0] == 0 && ratan_tbl[1024] == 512;
    for (i = 0; ok && i < 1024; i++) {
        ok = ratan_tbl[i] <= ratan_tbl[i + 1];
    }
    if (!ok) {
        SDL_Log("gte: the arctangent table in %s is not the expected one; ratan2 returns 0", EXE_NAME + 1);
        return -1;
    }
    ratan_state = 1;
    return 0;
}

const short *gte_table(int which, int *count) {
    tables_init();
    switch (which) {
    case GTE_TABLE_SIN:
        *count = 1025;
        return sin_tbl;
    case GTE_TABLE_COSSIN:
        *count = 4096 * 2;
        return cossin_tbl;
    case GTE_TABLE_SQRT:
        *count = 192;
        return sqrt_tbl;
    case GTE_TABLE_INV_SQRT:
        *count = 192;
        return inv_sqrt_tbl;
    case GTE_TABLE_ATAN:
        *count = 1025;
        return gte_ratan_table_load() == 0 ? ratan_tbl : NULL;
    default:
        *count = 0;
        return NULL;
    }
}

/* Logs `message` the first time its flag is clear. */
static void log_once(int *flag, const char *message) {
    if (!*flag) {
        *flag = 1;
        SDL_Log("%s", message);
    }
}

static int negate(int a) {
    return (int)(0u - (uint32_t)a);
}

/* The table index of a SquareRoot0 or VectorNormal argument, from its leading-bit count;
 * -1 when the PS1 would read outside the table (a negative argument). */
static int root_index(int value, int even_lzc) {
    int index;

    if (even_lzc >= 24) {
        index = (int)((uint32_t)value << (even_lzc - 24));
    } else {
        index = value >> (24 - even_lzc);
    }
    index -= 64;
    return index >= 0 && index < 192 ? index : -1;
}

void InitGeom(void) {
    tables_init();
    gte_init_geom();
}

static int sin_1(int a) {
    if (a <= 2048) {
        return a <= 1024 ? sin_tbl[a] : sin_tbl[2048 - a];
    }
    return a <= 3072 ? -sin_tbl[a - 2048] : -sin_tbl[4096 - a];
}

int rsin(int a) {
    tables_init();
    if (a >= 0) {
        return sin_1(a & 0xFFF);
    }
    return -sin_1(negate(a) & 0xFFF);
}

int rcos(int a) {
    tables_init();
    a = (a < 0 ? negate(a) : a) & 0xFFF;
    if (a <= 2048) {
        return a <= 1024 ? sin_tbl[1024 - a] : -sin_tbl[a - 1024];
    }
    return a <= 3072 ? -sin_tbl[3072 - a] : sin_tbl[a - 3072];
}

long ratan2(long y, long x) {
    static int warned;
    int neg_x;
    int neg_y;
    long v;
    int r;

    if (gte_ratan_table_load() != 0) {
        log_once(&warned, "gte: ratan2 without its table returns 0");
        return 0;
    }
    neg_x = x < 0;
    if (neg_x) {
        x = negate((int)x);
    }
    neg_y = y < 0;
    if (neg_y) {
        y = negate((int)y);
    }
    if (x == 0 && y == 0) {
        return 0;
    }
    if (y < x) {
        v = (y & 0x7FE00000) ? y / (x >> 10) : (y << 10) / x;
        r = ratan_tbl[v < 0 ? 0 : v > 1024 ? 1024 : v];
    } else {
        v = (x & 0x7FE00000) ? x / (y >> 10) : (x << 10) / y;
        r = 1024 - ratan_tbl[v < 0 ? 0 : v > 1024 ? 1024 : v];
    }
    if (neg_x) {
        r = 2048 - r;
    }
    return neg_y ? -r : r;
}

long SquareRoot0(long a) {
    static int warned;
    int lzc;
    int even;
    int index;

    tables_init();
    lzc = gte_lzc((int)a);
    if (lzc == 32) {
        return 0;
    }
    even = lzc & ~1;
    index = root_index((int)a, even);
    if (index < 0) {
        log_once(&warned, "gte: SquareRoot0 of a negative value; the PS1 reads outside its table, the host returns 0");
        return 0;
    }
    return (long)(((uint32_t)sqrt_tbl[index] << ((31 - even) >> 1)) >> 12);
}

/* Hyperbolic CORDIC, iterations 1 to 6 with the fourth done twice; the starting pair
 * is x +/- the library's constant. */
static int csqrt_1(int x) {
    int cx;
    int cy;
    int next;
    int i;
    int pass;

    cx = (int)((uint32_t)x + 0x5D50ADu);
    cy = (int)((uint32_t)x - 0x5D50ADu);
    for (i = 1; i <= 6; i++) {
        for (pass = 0; pass < (i == 4 ? 2 : 1); pass++) {
            if (cy >= 0) {
                next = cx - (cy >> i);
                cy -= cx >> i;
            } else {
                next = cx + (cy >> i);
                cy += cx >> i;
            }
            cx = next;
        }
    }
    return cx;
}

int csqrt(int a) {
    int d;
    int k;
    int x;
    int shift;
    int r;

    if (a == 0) {
        return 0;
    }
    d = 8 - gte_lzc(a);
    if (d >= 0) {
        k = d >> 1;
        x = a >> (2 * k);
    } else {
        k = (d >> 1) + 1;
        x = (int)((uint32_t)a << (-2 * k));
    }
    shift = k - 6;
    r = csqrt_1(x);
    return shift >= 0 ? (int)((uint32_t)r << shift) : r >> (-shift & 31);
}

/* Sine and cosine of an angle, from rcossin_tbl: a negative angle negates the sine. */
static void angle_sincos(int a, int *s, int *c) {
    int index;

    if (a >= 0) {
        index = a & 0xFFF;
        *s = cossin_tbl[index * 2];
    } else {
        index = negate(a) & 0xFFF;
        *s = -cossin_tbl[index * 2];
    }
    *c = cossin_tbl[index * 2 + 1];
}

MATRIX *RotMatrix(SVECTOR *r, MATRIX *m) {
    int sx, cx, sy, cy, sz, cz;
    int a;
    int b;

    tables_init();
    angle_sincos(r->vx, &sx, &cx);
    angle_sincos(r->vy, &sy, &cy);
    angle_sincos(r->vz, &sz, &cz);
    m->m[0][2] = (short)sy;
    m->m[1][2] = (short)(-(cy * sx) >> 12);
    m->m[2][2] = (short)((cy * cx) >> 12);
    m->m[0][0] = (short)((cz * cy) >> 12);
    m->m[0][1] = (short)(-(sz * cy) >> 12);
    a = (cz * -sy) >> 12;
    m->m[1][0] = (short)(((sz * cx) >> 12) - ((a * sx) >> 12));
    m->m[2][0] = (short)(((sz * sx) >> 12) + ((a * cx) >> 12));
    b = (sz * -sy) >> 12;
    m->m[1][1] = (short)(((cz * cx) >> 12) + ((b * sx) >> 12));
    m->m[2][1] = (short)(((cz * sx) >> 12) - ((b * cx) >> 12));
    return m;
}

MATRIX *RotMatrixZXY(SVECTOR *r, MATRIX *m) {
    int sx, cx, sy, cy, sz, cz;
    int a;
    int b;

    tables_init();
    angle_sincos(r->vx, &sx, &cx);
    angle_sincos(r->vy, &sy, &cy);
    angle_sincos(r->vz, &sz, &cz);
    m->m[2][1] = (short)sx;
    m->m[2][0] = (short)(-(cx * sy) >> 12);
    m->m[2][2] = (short)((cx * cy) >> 12);
    m->m[1][1] = (short)((cz * cx) >> 12);
    m->m[0][1] = (short)(-(sz * cx) >> 12);
    a = (sx * sy) >> 12;
    m->m[0][0] = (short)(((cz * cy) >> 12) - ((a * sz) >> 12));
    m->m[1][0] = (short)(((sz * cy) >> 12) + ((a * cz) >> 12));
    b = (sx * cy) >> 12;
    m->m[0][2] = (short)(((cz * sy) >> 12) + ((b * sz) >> 12));
    m->m[1][2] = (short)(((sz * sy) >> 12) - ((b * cz) >> 12));
    return m;
}

/* Rows p and q of m become (c * p - s * q, s * p + c * q) >> 12, all inputs read first. */
static void rotate_rows(MATRIX *m, int p, int q, int s, int c) {
    int rp[3];
    int rq[3];
    int j;

    for (j = 0; j < 3; j++) {
        rp[j] = m->m[p][j];
        rq[j] = m->m[q][j];
    }
    for (j = 0; j < 3; j++) {
        m->m[p][j] = (short)((c * rp[j] - s * rq[j]) >> 12);
        m->m[q][j] = (short)((s * rp[j] + c * rq[j]) >> 12);
    }
}

MATRIX *RotMatrixX(long r, MATRIX *m) {
    int s;
    int c;

    tables_init();
    angle_sincos((int)r, &s, &c);
    rotate_rows(m, 1, 2, s, c);
    return m;
}

MATRIX *RotMatrixY(long r, MATRIX *m) {
    int s;
    int c;

    tables_init();
    angle_sincos((int)r, &s, &c);
    rotate_rows(m, 0, 2, -s, c);
    return m;
}

MATRIX *RotMatrixZ(long r, MATRIX *m) {
    int s;
    int c;

    tables_init();
    angle_sincos((int)r, &s, &c);
    rotate_rows(m, 0, 1, s, c);
    return m;
}

MATRIX *TransMatrix(MATRIX *m, VECTOR *v) {
    m->t[0] = v->vx;
    m->t[1] = v->vy;
    m->t[2] = v->vz;
    return m;
}

void PushMatrix(void) {
    static int warned;

    if (matrix_depth >= MATRIX_STACK_DEPTH) {
        log_once(&warned, "Error: Can't push matrix,stack(max 20) is full!");
        return;
    }
    gte_h_ReadRotMatrix(&matrix_stack[matrix_depth++]);
    object_trace_push();
}

void PopMatrix(void) {
    static int warned;

    if (matrix_depth <= 0) {
        log_once(&warned, "Error: Can't pop matrix,stack is empty!");
        return;
    }
    matrix_depth--;
    object_trace_pop();
    SetRotMatrix(&matrix_stack[matrix_depth]);
    SetTransMatrix(&matrix_stack[matrix_depth]);
}

VECTOR *ApplyMatrixLV(MATRIX *m, VECTOR *v0, VECTOR *v1) {
    const long *in;
    int hi[3];
    int lo[3];
    int mac_hi[3];
    int mac_lo[3];
    int t;
    int i;

    gte_h_SetRotMatrix(m);
    in = &v0->vx;
    for (i = 0; i < 3; i++) {
        if (in[i] >= 0) {
            hi[i] = (int)in[i] >> 15;
            lo[i] = (int)in[i] & 0x7FFF;
        } else {
            t = negate((int)in[i]);
            hi[i] = negate(t >> 15);
            lo[i] = negate(t & 0x7FFF);
        }
    }
    gte_mvmva_rot(hi, 0, 0, 0, mac_hi);
    gte_mvmva_rot(lo, 0, 1, 0, mac_lo);
    v1->vx = (long)(int)((uint32_t)mac_lo[0] + ((uint32_t)mac_hi[0] << 3));
    v1->vy = (long)(int)((uint32_t)mac_lo[1] + ((uint32_t)mac_hi[1] << 3));
    v1->vz = (long)(int)((uint32_t)mac_lo[2] + ((uint32_t)mac_hi[2] << 3));
    return v1;
}

void RotTrans(SVECTOR *v0, VECTOR *v1, long *flag) {
    int v[3];
    int out[3];

    v[0] = v0->vx;
    v[1] = v0->vy;
    v[2] = v0->vz;
    gte_mvmva_rot(v, 1, 1, 0, out);
    v1->vx = out[0];
    v1->vy = out[1];
    v1->vz = out[2];
    *flag = (long)gte_flag_value();
}

void OuterProduct0(VECTOR *v0, VECTOR *v1, VECTOR *v2) {
    int d[3];
    int v[3];
    int out[3];

    d[0] = (int)v0->vx;
    d[1] = (int)v0->vy;
    d[2] = (int)v0->vz;
    v[0] = (int)v1->vx;
    v[1] = (int)v1->vy;
    v[2] = (int)v1->vz;
    gte_op(d, v, 0, 0, out);
    v2->vx = out[0];
    v2->vy = out[1];
    v2->vz = out[2];
}

/* VectorNormal's core: SQR, the inverse root at the squared length, GPF, then the
 * shift that undoes the table's scaling. Returns the squared length. */
static long normalize(int x, int y, int z, int out[3]) {
    int v[3];
    int sq[3];
    int scaled[3];
    uint32_t length2;
    int even;
    int index;
    int i;

    tables_init();
    v[0] = x;
    v[1] = y;
    v[2] = z;
    gte_sqr(v, 0, 1, sq);
    length2 = (uint32_t)sq[0] + (uint32_t)sq[1] + (uint32_t)sq[2];
    even = gte_lzc((int)length2) & ~1;
    index = root_index((int)length2, even);
    gte_gpf(index >= 0 ? inv_sqrt_tbl[index] : 0, v, 0, 0, scaled);
    for (i = 0; i < 3; i++) {
        out[i] = scaled[i] >> (((31 - even) >> 1) & 31);
    }
    return (long)(int)length2;
}

long VectorNormal(VECTOR *v0, VECTOR *v1) {
    int out[3];
    long length2;

    length2 = normalize((int)v0->vx, (int)v0->vy, (int)v0->vz, out);
    v1->vx = out[0];
    v1->vy = out[1];
    v1->vz = out[2];
    return length2;
}

long VectorNormalS(VECTOR *v0, SVECTOR *v1) {
    int out[3];
    long length2;

    length2 = normalize((int)v0->vx, (int)v0->vy, (int)v0->vz, out);
    v1->vx = (short)out[0];
    v1->vy = (short)out[1];
    v1->vz = (short)out[2];
    return length2;
}

long VectorNormalSS(SVECTOR *v0, SVECTOR *v1) {
    int out[3];
    long length2;

    length2 = normalize(v0->vx, v0->vy, v0->vz, out);
    v1->vx = (short)out[0];
    v1->vy = (short)out[1];
    v1->vz = (short)out[2];
    return length2;
}

void SetFarColor(long r, long g, long b) {
    gte_set_far_color((int)((uint32_t)r << 4), (int)((uint32_t)g << 4), (int)((uint32_t)b << 4));
}

void SetFogNear(long a, long h) {
    static int warned;

    if (h == 0) {
        log_once(&warned, "gte: SetFogNear with h = 0 (the PS1 traps); the depth cue is left as it was");
        return;
    }
    gte_set_depth_cue(negate((int)((uint32_t)a * 320u)) / (int)h, 0x1400000);
}
