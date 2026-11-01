/* libgte calls no game code makes, and the matrix loads that feed the host GTE
 * registers. The functions the game calls are in libgte_math.c. */

#include <libgte.h>
#include <inline_c.h>

long SquareRoot12(long a) {
    (void)a;
    return 0;
}

MATRIX *ScaleMatrix(MATRIX *m, VECTOR *v) {
    (void)v;
    return m;
}

MATRIX *MulMatrix(MATRIX *m0, MATRIX *m1) {
    (void)m1;
    return m0;
}

MATRIX *MulMatrix0(MATRIX *m0, MATRIX *m1, MATRIX *m2) {
    (void)m0;
    (void)m1;
    return m2;
}

MATRIX *CompMatrix(MATRIX *m0, MATRIX *m1, MATRIX *m2) {
    (void)m0;
    (void)m1;
    return m2;
}

void SetRotMatrix(MATRIX *m) {
    gte_h_SetRotMatrix(m);
}

void SetTransMatrix(MATRIX *m) {
    gte_h_SetTransMatrix(m);
}

VECTOR *ApplyMatrix(MATRIX *m, SVECTOR *v0, VECTOR *v1) {
    (void)m;
    (void)v0;
    return v1;
}

long RotTransPers(SVECTOR *v0, long *sxy, long *p, long *flag) {
    (void)v0;
    (void)sxy;
    (void)p;
    (void)flag;
    return 0;
}
