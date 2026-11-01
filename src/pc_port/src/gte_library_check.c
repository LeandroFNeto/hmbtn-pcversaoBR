/* The host GTE operations and libgte's CPU-side functions, checked at startup when
 * HMBTN_SELFTEST=1. Expected values come from the PSY-Q 4.7 LIBGTE code the game links
 * (pc-port-gte-library). The GTE matrix is saved and restored around the checks. */

#include <gte.h>

#include <SDL.h>

#include <string.h>

static int check(int ok, const char *what) {
    SDL_Log("gte library check %s: %s", ok ? "ok" : "FAILED", what);
    return ok ? 0 : 1;
}

static int check_ops(void) {
    static const int big[3] = {32767, -32768, 1000};
    static const int x_axis[3] = {1, 0, 0};
    static const int y_axis[3] = {0, 1, 0};
    MATRIX identity = {{{4096, 0, 0}, {0, 4096, 0}, {0, 0, 4096}}, {0, 0, 0}};
    int out[3];
    int failed;

    SetRotMatrix(&identity);
    SetTransMatrix(&identity);
    gte_mvmva_rot(big, 0, 0, 0, out);
    failed = check(out[0] == 32767 * 4096 && out[1] == -32768 * 4096 && out[2] == 1000 * 4096,
                   "MVMVA with sf=0 keeps 32-bit MAC results");
    failed |= check((gte_flag_value() & 0x81C00000u) == 0x81C00000u,
                    "MVMVA with sf=0 flags IR1-3 saturation and the error bit");
    gte_op(x_axis, y_axis, 0, 0, out);
    failed |= check(out[0] == 0 && out[1] == 0 && out[2] == 1 && gte_flag_value() == 0,
                    "OP with sf=0: (1, 0, 0) x (0, 1, 0) = (0, 0, 1)");
    failed |= check(gte_lzc(1) == 31 && gte_lzc(0x7FFFFFFF) == 1 && gte_lzc(-1) == 32 && gte_lzc(0) == 32 &&
                        gte_lzc((int)0x80000000u) == 1,
                    "leading-bit count of 1, 0x7FFFFFFF, -1, 0, 0x80000000");
    return failed;
}

/* The computed tables against the executable, and the arctangent table read from it. */
static int check_tables(void) {
    static const struct {
        int which;
        unsigned int offset;
        const char *name;
    } tables[] = {
        {GTE_TABLE_SIN, 0x48254u, "rsin_tbl"},
        {GTE_TABLE_COSSIN, 0x490C4u, "rcossin_tbl"},
        {GTE_TABLE_SQRT, 0x48A94u, "SQRT"},
        {GTE_TABLE_INV_SQRT, 0x48C28u, "the inverse root table"},
    };
    static short exe[4096 * 2];
    char what[96];
    const short *table;
    int compared;
    int count;
    int failed;
    int same;
    int i;
    int k;

    failed = 0;
    compared = 0;
    for (i = 0; i < (int)(sizeof(tables) / sizeof(tables[0])); i++) {
        table = gte_table(tables[i].which, &count);
        same = gte_exe_read(tables[i].offset, exe, (uint32_t)count * 2) == 0;
        for (k = 0; same && k < count; k++) {
            same = table[k] == exe[k];
        }
        SDL_snprintf(what, sizeof(what), "%s equals the executable's (%d entries)", tables[i].name, count);
        failed |= check(same, what);
        compared += same ? count : 0;
    }
    table = gte_table(GTE_TABLE_ATAN, &count);
    failed |= check(table != NULL && table[0] == 0 && table[1024] == 512,
                    "ratan_tbl is read from the executable, 0 to 512");
    SDL_Log("gte library check: %d table entries compared with SLUS_011.15", compared);
    return failed;
}

static int check_angles(void) {
    int failed;

    failed = check(rsin(0) == 0 && rsin(512) == 2896 && rsin(1024) == 4096 && rsin(3072) == -4096 &&
                       rsin(-1024) == -4096 && rsin(4608) == 2896,
                   "rsin of 0, 512, 1024, 3072, -1024, 4608");
    failed |= check(rcos(0) == 4096 && rcos(2048) == -4096, "rcos of 0 and 2048");
    failed |= check(ratan2(0, 1) == 0 && ratan2(1, 1) == 512 && ratan2(1, 0) == 1024 && ratan2(-1, 0) == -1024,
                    "ratan2 quadrants");
    failed |= check(ratan2(1, 2) == 302 && ratan2(-300, -700) == -1784 && ratan2(5000, 123) == 1008,
                    "ratan2 of (1, 2), (-300, -700), (5000, 123)");
    return failed;
}

static int check_roots(void) {
    int failed;

    failed = check(SquareRoot0(0) == 0 && SquareRoot0(65536) == 256 && SquareRoot0(1073741824) == 32768,
                   "SquareRoot0 of 0, 65536, 2^30");
    failed |= check(SquareRoot0(1000000) == 999 && SquareRoot0(12345678) == 3510 && SquareRoot0(2) == 1,
                    "SquareRoot0 keeps the table's rounding (1000000 -> 999)");
    failed |= check(csqrt(0) == 0 && csqrt(4096) == 4096 && csqrt(16384) == 8192 && csqrt(1000000) == 64002 &&
                        csqrt(2) == 90,
                    "csqrt (20.12) of 0, 1.0, 4.0, 1000000, 2");
    return failed;
}

static int matrix_is(const MATRIX *m, const short *expected) {
    int r;
    int c;

    for (r = 0; r < 3; r++) {
        for (c = 0; c < 3; c++) {
            if (m->m[r][c] != expected[r * 3 + c]) {
                return 0;
            }
        }
    }
    return 1;
}

static int check_matrices(void) {
    static const short identity3[3][3] = {{4096, 0, 0}, {0, 4096, 0}, {0, 0, 4096}};
    static const short quarter_x[3][3] = {{4096, 0, 0}, {0, 0, -4096}, {0, 4096, 0}};
    static const short rotated[3][3] = {{3711, -739, 1567}, {1651, 2623, -2676}, {-523, 3057, 2675}};
    SVECTOR angles;
    VECTOR shift = {7, 8, 9, 0};
    MATRIX m;
    int failed;

    memset(&m, 0, sizeof(m));
    angles.vx = angles.vy = angles.vz = 0;
    RotMatrix(&angles, &m);
    failed = check(matrix_is(&m, &identity3[0][0]), "RotMatrix of (0, 0, 0) is the identity");
    angles.vx = 512;
    angles.vy = 256;
    angles.vz = 128;
    RotMatrix(&angles, &m);
    failed |= check(matrix_is(&m, &rotated[0][0]), "RotMatrix of (512, 256, 128)");
    memset(&m, 0, sizeof(m));
    m.m[0][0] = m.m[1][1] = m.m[2][2] = 4096;
    RotMatrixX(1024, &m);
    failed |= check(matrix_is(&m, &quarter_x[0][0]), "RotMatrixX(1024) of the identity");
    RotMatrixX(-1024, &m);
    RotMatrixY(1024, &m);
    RotMatrixY(-1024, &m);
    RotMatrixZ(2048, &m);
    RotMatrixZ(-2048, &m);
    failed |= check(matrix_is(&m, &identity3[0][0]), "RotMatrixX, Y, Z and their inverses give back the identity");
    angles.vx = angles.vy = angles.vz = 0;
    RotMatrixZXY(&angles, &m);
    failed |= check(matrix_is(&m, &identity3[0][0]), "RotMatrixZXY of (0, 0, 0) is the identity");
    TransMatrix(&m, &shift);
    failed |= check(m.t[0] == 7 && m.t[1] == 8 && m.t[2] == 9, "TransMatrix sets the translation");
    return failed;
}

static int check_stack_and_vectors(void) {
    MATRIX a = {{{4096, 0, 0}, {0, 4096, 0}, {0, 0, 4096}}, {10, 20, 30}};
    MATRIX b = {{{0, 4096, 0}, {4096, 0, 0}, {0, 0, 4096}}, {-1, -2, -3}};
    MATRIX current;
    SVECTOR small = {1, 2, 3, 0};
    VECTOR big = {100000, -200000, 300000, 0};
    VECTOR x_axis = {1, 0, 0, 0};
    VECTOR y_axis = {0, 1, 0, 0};
    VECTOR v345 = {3, 4, 0, 0};
    VECTOR vlong = {100, 200, 300, 0};
    VECTOR out;
    SVECTOR sout;
    long flag;
    long length2;
    int failed;

    SetRotMatrix(&a);
    SetTransMatrix(&a);
    PushMatrix();
    SetRotMatrix(&b);
    SetTransMatrix(&b);
    PopMatrix();
    gte_h_ReadRotMatrix(&current);
    failed = check(matrix_is(&current, &a.m[0][0]) && current.t[0] == 10 && current.t[2] == 30,
                   "PopMatrix restores the pushed rotation and translation");
    RotTrans(&small, &out, &flag);
    failed |= check(out.vx == 11 && out.vy == 22 && out.vz == 33 && flag == 0, "RotTrans of (1, 2, 3) by +(10, 20, 30)");
    ApplyMatrixLV(&a, &big, &out);
    failed |= check(out.vx == 100000 && out.vy == -200000 && out.vz == 300000, "ApplyMatrixLV keeps 32-bit components");
    ApplyMatrixLV(&b, &big, &out);
    gte_h_ReadRotMatrix(&current);
    failed |= check(out.vx == -200000 && out.vy == 100000 && matrix_is(&current, &b.m[0][0]),
                    "ApplyMatrixLV swaps x and y with b, and leaves b as the current rotation");
    SetRotMatrix(&a);
    OuterProduct0(&x_axis, &y_axis, &out);
    gte_h_ReadRotMatrix(&current);
    failed |= check(out.vx == 0 && out.vy == 0 && out.vz == 1 && matrix_is(&current, &a.m[0][0]),
                    "OuterProduct0 of x and y is z, the rotation unchanged");
    length2 = VectorNormal(&v345, &out);
    failed |= check(out.vx == 2457 && out.vy == 3276 && out.vz == 0 && length2 == 25,
                    "VectorNormal of (3, 4, 0) is (2457, 3276, 0), returning 25");
    VectorNormalS(&vlong, &sout);
    failed |= check(sout.vx == 1097 && sout.vy == 2194 && sout.vz == 3291, "VectorNormalS of (100, 200, 300)");
    small.vx = 3;
    small.vy = 4;
    small.vz = 0;
    VectorNormalSS(&small, &sout);
    failed |= check(sout.vx == 2457 && sout.vy == 3276 && sout.vz == 0, "VectorNormalSS of (3, 4, 0)");
    return failed;
}

static int check_settings(void) {
    GteSettings before;
    GteSettings s;
    int failed;

    gte_debug_settings(&before);
    InitGeom();
    gte_debug_settings(&s);
    failed = check(s.screen_h == 1000 && s.off_x == 0 && s.off_y == 0 && s.dqa == -4194 && s.dqb == 0x1400000 &&
                       s.zsf3 == 341 && s.zsf4 == 256,
                   "InitGeom sets H 1000, offset 0, DQA -4194, DQB 0x1400000, ZSF3 341, ZSF4 256");
    SetFogNear(100, 1000);
    SetFarColor(1, 2, 3);
    gte_debug_settings(&s);
    failed |= check(s.dqa == -32 && s.dqb == 0x1400000 && s.far_color[0] == 16 && s.far_color[1] == 32 &&
                        s.far_color[2] == 48,
                    "SetFogNear(100, 1000) gives DQA -32; SetFarColor(1, 2, 3) gives 16, 32, 48");
    {
        MATRIX color = {{{1, 2, 3}, {4, 5, 6}, {7, 8, 9}}, {0, 0, 0}};
        MATRIX light = {{{9, 8, 7}, {6, 5, 4}, {3, 2, 1}}, {99, 99, 99}};
        int r;
        int c;
        int matrices;

        gte_SetBackColor(10, 20, 30);
        gte_SetColorMatrix(&color);
        gte_SetLightMatrix(&light);
        gte_debug_settings(&s);
        matrices = 1;
        for (r = 0; r < 3; r++) {
            for (c = 0; c < 3; c++) {
                matrices &= s.color_matrix[r][c] == color.m[r][c] && s.light_matrix[r][c] == light.m[r][c];
            }
        }
        failed |= check(s.back_color[0] == 10 && s.back_color[1] == 20 && s.back_color[2] == 30 && matrices,
                        "SetBackColor(10, 20, 30) reads (10, 20, 30); both matrices read back");
        gte_SetBackColor(before.back_color[0], before.back_color[1], before.back_color[2]);
        gte_SetColorMatrix(&before.color_matrix);
        gte_SetLightMatrix(&before.light_matrix);
    }
    SetGeomScreen(before.screen_h);
    SetGeomOffset(before.off_x, before.off_y);
    gte_set_depth_cue(before.dqa, before.dqb);
    gte_set_far_color(before.far_color[0], before.far_color[1], before.far_color[2]);
    return failed;
}

int gte_library_self_check(void) {
    MATRIX saved;
    int failed;

    gte_h_ReadRotMatrix(&saved);
    failed = check_ops();
    failed |= check_tables();
    failed |= check_angles();
    failed |= check_roots();
    failed |= check_matrices();
    failed |= check_stack_and_vectors();
    failed |= check_settings();
    SetRotMatrix(&saved);
    SetTransMatrix(&saved);
    return failed;
}
