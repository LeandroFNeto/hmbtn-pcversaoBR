#ifndef INLINE_C_H
#define INLINE_C_H

/* Host GTE. The PSY-Q register macros become calls into src/pc_port/src/gte.c, which
 * keeps the coprocessor registers in C. Nothing here emits a MIPS opcode. */

void gte_h_ldv0(const void *v);
void gte_h_ldv3(const void *v0, const void *v1, const void *v2);
void gte_h_ldlv0(const void *v);
void gte_h_ldlvl(const void *v);
void gte_h_ldclmv(const void *m);
void gte_h_rtps(void);
void gte_h_rtpt(void);
void gte_h_rtv0(void);
void gte_h_rtir(void);
void gte_h_nclip(void);
void gte_h_avsz3(void);
void gte_h_avsz4(void);
void gte_h_sqr0(void);
void gte_h_op12(void);
void gte_h_stsxy(void *p);
void gte_h_stsxy3(void *p, int off0, int off1, int off2);
void gte_h_stflg(void *p);
void gte_h_stopz(void *p);
void gte_h_stotz(void *p);
void gte_h_stszotz(void *p);
void gte_h_stlvl(void *p);
void gte_h_stlvnl(void *p);
void gte_h_stclmv(void *m);
void gte_h_SetRotMatrix(const void *m);
void gte_h_SetTransMatrix(const void *m);
void gte_h_ReadRotMatrix(void *m);
void gte_h_SetBackColor(int r, int g, int b);
void gte_h_SetColorMatrix(const void *m);
void gte_h_SetLightMatrix(const void *m);

#define gte_ldv0(r0) gte_h_ldv0(r0)
#define gte_ldv3(r0, r1, r2) gte_h_ldv3(r0, r1, r2)
#define gte_ldlv0(r0) gte_h_ldlv0(r0)
#define gte_ldlvl(r0) gte_h_ldlvl(r0)
#define gte_ldclmv(r0) gte_h_ldclmv(r0)

#define gte_rtps() gte_h_rtps()
#define gte_rtpt() gte_h_rtpt()
#define gte_rtps_b() gte_h_rtps()
#define gte_rtpt_b() gte_h_rtpt()
#define gte_rtv0() gte_h_rtv0()
#define gte_rtir() gte_h_rtir()
#define gte_nclip() gte_h_nclip()
#define gte_nclip_b() gte_h_nclip()
#define gte_avsz3() gte_h_avsz3()
#define gte_avsz4() gte_h_avsz4()
#define gte_avsz3_b() gte_h_avsz3()
#define gte_avsz4_b() gte_h_avsz4()
#define gte_sqr0() gte_h_sqr0()
#define gte_sqr0_b() gte_h_sqr0()
#define gte_op12() gte_h_op12()

#define gte_stsxy(r0) gte_h_stsxy(r0)
#define gte_stsxy2(r0) gte_h_stsxy(r0)
#define gte_stsxy3_f3(r0) gte_h_stsxy3(r0, 8, 12, 16)
#define gte_stsxy3_f4(r0) gte_h_stsxy3(r0, 8, 12, 16)
#define gte_stsxy3_ft3(r0) gte_h_stsxy3(r0, 8, 16, 24)
#define gte_stsxy3_ft4(r0) gte_h_stsxy3(r0, 8, 16, 24)
#define gte_stsxy3_g3(r0) gte_h_stsxy3(r0, 8, 16, 24)
#define gte_stsxy3_g4(r0) gte_h_stsxy3(r0, 8, 16, 24)
#define gte_stsxy3_gt3(r0) gte_h_stsxy3(r0, 8, 20, 32)
#define gte_stsxy3_gt4(r0) gte_h_stsxy3(r0, 8, 20, 32)
#define gte_stflg(r0) gte_h_stflg(r0)
#define gte_stopz(r0) gte_h_stopz(r0)
#define gte_stotz(r0) gte_h_stotz(r0)
#define gte_stszotz(r0) gte_h_stszotz(r0)
#define gte_stlvl(r0) gte_h_stlvl(r0)
#define gte_stlvnl(r0) gte_h_stlvnl(r0)
#define gte_stclmv(r0) gte_h_stclmv(r0)

#define gte_SetRotMatrix(r0) gte_h_SetRotMatrix(r0)
#define gte_SetTransMatrix(r0) gte_h_SetTransMatrix(r0)
#define gte_ReadRotMatrix(r0) gte_h_ReadRotMatrix(r0)
#define gte_SetBackColor(r0, r1, r2) gte_h_SetBackColor(r0, r1, r2)
#define gte_SetColorMatrix(r0) gte_h_SetColorMatrix(r0)
#define gte_SetLightMatrix(r0) gte_h_SetLightMatrix(r0)

#endif
