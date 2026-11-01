#ifndef PC_PORT_GTE_H
#define PC_PORT_GTE_H

/* Host GTE. Replaces src/include/gte.h on the host include path: the command macros
 * come from the host inline_c.h as C calls. Nothing here emits a MIPS opcode. */

#include <inline_c.h>
#include <libgte.h>

#include <stdint.h>

void gte_set_rotation_identity(void);
void gte_set_translation(int tx, int ty, int tz);

/* Writes signed 16-bit screen coordinates. Returns 0 when z is in front,
 * and -1 when the vertex fails the front-of-camera test. */
int gte_project(int vx, int vy, int vz, short *sx, short *sy);

/* Identity at (160, 120), a vertex to the right, and a vertex behind. */
int gte_startup_check(void);

/* The GTE operations libgte's CPU-side functions run. Each loads its vector inputs as
 * the 16-bit registers do (the low 16 bits, sign-extended), clears and sets FLAG, leaves
 * MAC1-3 and IR1-3 as the hardware does, and writes MAC1-3 to `out`. sf shifts the result
 * right by 12; lm saturates IR to 0..0x7FFF instead of -0x8000..0x7FFF. */
void gte_mvmva_rot(const int v[3], int translate, int sf, int lm, int out[3]); /* R*v (+TR) */
void gte_op(const int d[3], const int v[3], int sf, int lm, int out[3]);      /* d x v */
void gte_sqr(const int v[3], int sf, int lm, int out[3]);                     /* v*v */
void gte_gpf(int ir0, const int v[3], int sf, int lm, int out[3]);            /* ir0*v */
/* LZCR for LZCS = value: leading bits equal to bit 31; 32 for 0 and -1. */
int gte_lzc(int value);
unsigned int gte_flag_value(void);

/* InitGeom's register values, and the far color and depth-cue registers. */
void gte_init_geom(void);
void gte_set_far_color(int r, int g, int b);
void gte_set_depth_cue(int dqa, int dqb);

typedef struct {
    int screen_h;
    int off_x;
    int off_y;
    int dqa;
    int dqb;
    int zsf3;
    int zsf4;
    int far_color[3];
    int back_color[3];
    short color_matrix[3][3];
    short light_matrix[3][3];
} GteSettings;

void gte_debug_settings(GteSettings *s);

/* libgte's lookup tables (libgte_math.c), for checks. The arctangent table comes from the
 * game's executable; it is NULL when that cannot be read. */
enum { GTE_TABLE_SIN, GTE_TABLE_COSSIN, GTE_TABLE_SQRT, GTE_TABLE_INV_SQRT, GTE_TABLE_ATAN };
const short *gte_table(int which, int *count);
/* Loads the arctangent table once the CD drive is open. Returns 0, or -1 after logging. */
int gte_ratan_table_load(void);
/* Copies `bytes` of the game's executable on the disc from file offset `offset`.
 * Returns 0, or -1 when the drive or the file is missing. */
int gte_exe_read(uint32_t offset, void *dst, uint32_t bytes);

#endif
