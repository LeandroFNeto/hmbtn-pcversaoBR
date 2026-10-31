/* libgte calls outside the projection in gte.c. The matrix loads feed the host GTE
 * registers; the math helpers return zero until a later change implements them. */

#include <libgte.h>
#include <inline_c.h>

void InitGeom(void) {
}

long ratan2(long y, long x) {
    (void)y;
    (void)x;
    return 0;
}

long csqrt(long a) {
    (void)a;
    return 0;
}

long SquareRoot0(long a) {
    (void)a;
    return 0;
}

long SquareRoot12(long a) {
    (void)a;
    return 0;
}

int rsin(int a) {
    (void)a;
    return 0;
}

int rcos(int a) {
    (void)a;
    return 0;
}

MATRIX *RotMatrix(SVECTOR *r, MATRIX *m) {
    (void)r;
    return m;
}

MATRIX *RotMatrixX(long r, MATRIX *m) {
    (void)r;
    return m;
}

MATRIX *RotMatrixY(long r, MATRIX *m) {
    (void)r;
    return m;
}

MATRIX *RotMatrixZ(long r, MATRIX *m) {
    (void)r;
    return m;
}

MATRIX *RotMatrixZXY(SVECTOR *r, MATRIX *m) {
    (void)r;
    return m;
}

MATRIX *TransMatrix(MATRIX *m, VECTOR *v) {
    (void)v;
    return m;
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

void PushMatrix(void) {
}

void PopMatrix(void) {
}

VECTOR *ApplyMatrix(MATRIX *m, SVECTOR *v0, VECTOR *v1) {
    (void)m;
    (void)v0;
    return v1;
}

VECTOR *ApplyMatrixLV(MATRIX *m, VECTOR *v0, VECTOR *v1) {
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

VECTOR *RotTrans(SVECTOR *v0, VECTOR *v1, long *flag) {
    (void)v0;
    (void)flag;
    return v1;
}

void OuterProduct0(VECTOR *v0, VECTOR *v1, VECTOR *v2) {
    (void)v0;
    (void)v1;
    (void)v2;
}

void VectorNormal(VECTOR *v0, VECTOR *v1) {
    (void)v0;
    (void)v1;
}

void VectorNormalS(VECTOR *v0, SVECTOR *v1) {
    (void)v0;
    (void)v1;
}

void VectorNormalSS(SVECTOR *v0, SVECTOR *v1) {
    (void)v0;
    (void)v1;
}

void SetFarColor(long r, long g, long b) {
    (void)r;
    (void)g;
    (void)b;
}

void SetFogNear(long a, long h) {
    (void)a;
    (void)h;
}
