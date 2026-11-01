#ifndef LIBGTE_H
#define LIBGTE_H

#include <sys/types.h>

/* PSY-Q 4.7 layouts. `long` is 32 bits on the i686 host, as on the R3000. */
typedef struct {
    short m[3][3];
    long t[3];
} MATRIX;

typedef struct {
    long vx, vy, vz, pad;
} VECTOR;

typedef struct {
    short vx, vy, vz, pad;
} SVECTOR;

typedef struct {
    u_char r, g, b, cd;
} CVECTOR;

typedef struct {
    short vx, vy;
} DVECTOR;

#define ONE 4096

void InitGeom(void);
void SetGeomOffset(long ofx, long ofy);
void SetGeomScreen(long h);
void SetFarColor(long rfc, long gfc, long bfc);
void SetFogNear(long a, long h);
long ratan2(long y, long x);
int csqrt(int a);
long SquareRoot0(long a);
long SquareRoot12(long a);
int rsin(int a);
int rcos(int a);
MATRIX *RotMatrix(SVECTOR *r, MATRIX *m);
MATRIX *RotMatrixZXY(SVECTOR *r, MATRIX *m);
MATRIX *RotMatrixX(long r, MATRIX *m);
MATRIX *RotMatrixY(long r, MATRIX *m);
MATRIX *RotMatrixZ(long r, MATRIX *m);
MATRIX *TransMatrix(MATRIX *m, VECTOR *v);
MATRIX *ScaleMatrix(MATRIX *m, VECTOR *v);
MATRIX *MulMatrix(MATRIX *m0, MATRIX *m1);
MATRIX *MulMatrix0(MATRIX *m0, MATRIX *m1, MATRIX *m2);
MATRIX *CompMatrix(MATRIX *m0, MATRIX *m1, MATRIX *m2);
void SetRotMatrix(MATRIX *m);
void SetTransMatrix(MATRIX *m);
void PushMatrix(void);
void PopMatrix(void);
VECTOR *ApplyMatrix(MATRIX *m, SVECTOR *v0, VECTOR *v1);
VECTOR *ApplyMatrixLV(MATRIX *m, VECTOR *v0, VECTOR *v1);
void RotTrans(SVECTOR *v0, VECTOR *v1, long *flag);
long RotTransPers(SVECTOR *v0, long *sxy, long *p, long *flag);
void OuterProduct0(VECTOR *v0, VECTOR *v1, VECTOR *v2);
long VectorNormal(VECTOR *v0, VECTOR *v1);
long VectorNormalS(VECTOR *v0, SVECTOR *v1);
long VectorNormalSS(SVECTOR *v0, SVECTOR *v1);

#endif
