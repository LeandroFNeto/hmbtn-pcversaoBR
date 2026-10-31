#ifndef GTEMAC_H
#define GTEMAC_H

#include <inline_c.h>

void gte_h_MulMatrix0(const void *m0, const void *m1, void *m2);
void gte_h_OuterProduct12(const void *v0, const void *v1, void *v2);

#define gte_MulMatrix0(r1, r2, r3) gte_h_MulMatrix0(r1, r2, r3)
#define gte_OuterProduct12(r1, r2, r3) gte_h_OuterProduct12(r1, r2, r3)

#endif
