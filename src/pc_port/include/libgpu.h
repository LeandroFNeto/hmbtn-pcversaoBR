#ifndef LIBGPU_H
#define LIBGPU_H

#include <sys/types.h>

/* PSY-Q 4.7 layouts and macros. Packet sizes match the SDK, because the game lays
 * packets out at fixed offsets in its work region. */

typedef struct {
    short x, y, w, h;
} RECT;

typedef struct {
    unsigned addr : 24;
    unsigned len : 8;
    u_char r0, g0, b0, code;
} P_TAG;

typedef struct {
    u_char r0, g0, b0, code;
} P_CODE;

typedef struct {
    u_long tag;
    u_long code[15];
} DR_ENV;

typedef struct {
    RECT clip;
    short ofs[2];
    RECT tw;
    u_short tpage;
    u_char dtd, dfe, isbg, r0, g0, b0;
    DR_ENV dr_env;
} DRAWENV;

typedef struct {
    RECT disp;
    RECT screen;
    u_char isinter, isrgb24, pad0, pad1;
} DISPENV;

typedef struct {
    u_long tag;
    u_char r0, g0, b0, code;
    short x0, y0;
    short x1, y1;
    short x2, y2;
} POLY_F3;

typedef struct {
    u_long tag;
    u_char r0, g0, b0, code;
    short x0, y0;
    short x1, y1;
    short x2, y2;
    short x3, y3;
} POLY_F4;

typedef struct {
    u_long tag;
    u_char r0, g0, b0, code;
    short x0, y0;
    u_char u0, v0;
    u_short clut;
    short x1, y1;
    u_char u1, v1;
    u_short tpage;
    short x2, y2;
    u_char u2, v2;
    u_short pad1;
} POLY_FT3;

typedef struct {
    u_long tag;
    u_char r0, g0, b0, code;
    short x0, y0;
    u_char u0, v0;
    u_short clut;
    short x1, y1;
    u_char u1, v1;
    u_short tpage;
    short x2, y2;
    u_char u2, v2;
    u_short pad1;
    short x3, y3;
    u_char u3, v3;
    u_short pad2;
} POLY_FT4;

typedef struct {
    u_long tag;
    u_char r0, g0, b0, code;
    short x0, y0;
    u_char r1, g1, b1, pad1;
    short x1, y1;
    u_char r2, g2, b2, pad2;
    short x2, y2;
} POLY_G3;

typedef struct {
    u_long tag;
    u_char r0, g0, b0, code;
    short x0, y0;
    u_char r1, g1, b1, pad1;
    short x1, y1;
    u_char r2, g2, b2, pad2;
    short x2, y2;
    u_char r3, g3, b3, pad3;
    short x3, y3;
} POLY_G4;

typedef struct {
    u_long tag;
    u_char r0, g0, b0, code;
    short x0, y0;
    u_char u0, v0;
    u_short clut;
    u_char r1, g1, b1, p1;
    short x1, y1;
    u_char u1, v1;
    u_short tpage;
    u_char r2, g2, b2, p2;
    short x2, y2;
    u_char u2, v2;
    u_short pad2;
} POLY_GT3;

typedef struct {
    u_long tag;
    u_char r0, g0, b0, code;
    short x0, y0;
    u_char u0, v0;
    u_short clut;
    u_char r1, g1, b1, p1;
    short x1, y1;
    u_char u1, v1;
    u_short tpage;
    u_char r2, g2, b2, p2;
    short x2, y2;
    u_char u2, v2;
    u_short pad2;
    u_char r3, g3, b3, p3;
    short x3, y3;
    u_char u3, v3;
    u_short pad3;
} POLY_GT4;

typedef struct {
    u_long tag;
    u_char r0, g0, b0, code;
    short x0, y0;
    short x1, y1;
} LINE_F2;

typedef struct {
    u_long tag;
    u_char r0, g0, b0, code;
    short x0, y0;
    u_char r1, g1, b1, p1;
    short x1, y1;
} LINE_G2;

typedef struct {
    u_long tag;
    u_char r0, g0, b0, code;
    short x0, y0;
    short x1, y1;
    short x2, y2;
    u_long pad;
} LINE_F3;

typedef struct {
    u_long tag;
    u_char r0, g0, b0, code;
    short x0, y0;
    short x1, y1;
    short x2, y2;
    short x3, y3;
    u_long pad;
} LINE_F4;

typedef struct {
    u_long tag;
    u_char r0, g0, b0, code;
    short x0, y0;
    u_char u0, v0;
    u_short clut;
    short w, h;
} SPRT;

typedef struct {
    u_long tag;
    u_char r0, g0, b0, code;
    short x0, y0;
    u_char u0, v0;
    u_short clut;
} SPRT_16;

typedef struct {
    u_long tag;
    u_char r0, g0, b0, code;
    short x0, y0;
    u_char u0, v0;
    u_short clut;
} SPRT_8;

typedef struct {
    u_long tag;
    u_char r0, g0, b0, code;
    short x0, y0;
    short w, h;
} TILE;

typedef struct {
    u_long tag;
    u_char r0, g0, b0, code;
    short x0, y0;
} TILE_16;

typedef struct {
    u_long tag;
    u_char r0, g0, b0, code;
    short x0, y0;
} TILE_8;

typedef struct {
    u_long tag;
    u_char r0, g0, b0, code;
    short x0, y0;
} TILE_1;

typedef struct {
    u_long tag;
    u_long code[2];
} DR_MODE;

typedef struct {
    u_long tag;
    u_long code[2];
} DR_TWIN;

typedef struct {
    u_long tag;
    u_long code[2];
} DR_AREA;

typedef struct {
    u_long tag;
    u_long code[2];
} DR_OFFSET;

typedef struct {
    u_long tag;
    u_long code[5];
} DR_MOVE;

typedef struct {
    u_long tag;
    u_long code[3];
    u_long p[13];
} DR_LOAD;

typedef struct {
    u_long tag;
    u_long code[1];
} DR_TPAGE;

typedef struct {
    u_long tag;
    u_long code[2];
} DR_STP;

#define setVector(v, _x, _y, _z) (v)->vx = (_x), (v)->vy = (_y), (v)->vz = (_z)
#define setRECT(r, _x, _y, _w, _h) (r)->x = (_x), (r)->y = (_y), (r)->w = (_w), (r)->h = (_h)

#define getTPage(tp, abr, x, y)                                                     \
    ((((tp) & 0x3) << 7) | (((abr) & 0x3) << 5) | (((y) & 0x100) >> 4) |           \
     (((x) & 0x3ff) >> 6) | (((y) & 0x200) << 2))
#define getClut(x, y) (((y) << 6) | (((x) >> 4) & 0x3f))

#define setTPage(p, tp, abr, x, y) ((p)->tpage = getTPage(tp, abr, x, y))
#define setClut(p, x, y) ((p)->clut = getClut(x, y))

#define setRGB0(p, _r0, _g0, _b0) (p)->r0 = (_r0), (p)->g0 = (_g0), (p)->b0 = (_b0)
#define setRGB1(p, _r1, _g1, _b1) (p)->r1 = (_r1), (p)->g1 = (_g1), (p)->b1 = (_b1)
#define setRGB2(p, _r2, _g2, _b2) (p)->r2 = (_r2), (p)->g2 = (_g2), (p)->b2 = (_b2)
#define setRGB3(p, _r3, _g3, _b3) (p)->r3 = (_r3), (p)->g3 = (_g3), (p)->b3 = (_b3)

#define setXY0(p, _x0, _y0) (p)->x0 = (_x0), (p)->y0 = (_y0)
#define setXY2(p, _x0, _y0, _x1, _y1) (p)->x0 = (_x0), (p)->y0 = (_y0), (p)->x1 = (_x1), (p)->y1 = (_y1)
#define setXY3(p, _x0, _y0, _x1, _y1, _x2, _y2)                                     \
    (p)->x0 = (_x0), (p)->y0 = (_y0), (p)->x1 = (_x1), (p)->y1 = (_y1),             \
    (p)->x2 = (_x2), (p)->y2 = (_y2)
#define setXY4(p, _x0, _y0, _x1, _y1, _x2, _y2, _x3, _y3)                           \
    (p)->x0 = (_x0), (p)->y0 = (_y0), (p)->x1 = (_x1), (p)->y1 = (_y1),             \
    (p)->x2 = (_x2), (p)->y2 = (_y2), (p)->x3 = (_x3), (p)->y3 = (_y3)
#define setXYWH(p, _x0, _y0, _w, _h)                                                \
    (p)->x0 = (_x0), (p)->y0 = (_y0), (p)->x1 = (_x0) + (_w), (p)->y1 = (_y0),      \
    (p)->x2 = (_x0), (p)->y2 = (_y0) + (_h), (p)->x3 = (_x0) + (_w), (p)->y3 = (_y0) + (_h)
#define setWH(p, _w, _h) (p)->w = (_w), (p)->h = (_h)

#define setUV0(p, _u0, _v0) (p)->u0 = (_u0), (p)->v0 = (_v0)
#define setUV3(p, _u0, _v0, _u1, _v1, _u2, _v2)                                     \
    (p)->u0 = (_u0), (p)->v0 = (_v0), (p)->u1 = (_u1), (p)->v1 = (_v1),             \
    (p)->u2 = (_u2), (p)->v2 = (_v2)
#define setUV4(p, _u0, _v0, _u1, _v1, _u2, _v2, _u3, _v3)                           \
    (p)->u0 = (_u0), (p)->v0 = (_v0), (p)->u1 = (_u1), (p)->v1 = (_v1),             \
    (p)->u2 = (_u2), (p)->v2 = (_v2), (p)->u3 = (_u3), (p)->v3 = (_v3)
#define setUVWH(p, _u0, _v0, _w, _h)                                                \
    (p)->u0 = (_u0), (p)->v0 = (_v0), (p)->u1 = (_u0) + (_w), (p)->v1 = (_v0),      \
    (p)->u2 = (_u0), (p)->v2 = (_v0) + (_h), (p)->u3 = (_u0) + (_w), (p)->v3 = (_v0) + (_h)

#define setaddr(p, _addr) (((P_TAG *)(p))->addr = (u_long)(uintptr_t)(_addr))
#define setlen(p, _len) (((P_TAG *)(p))->len = (u_char)(_len))
#define setcode(p, _code) (((P_TAG *)(p))->code = (u_char)(_code))
#define getlen(p) (u_char)(((P_TAG *)(p))->len)
#define getcode(p) (u_char)(((P_TAG *)(p))->code)
#define getaddr(p) (u_long)(((P_TAG *)(p))->addr)

#define addPrim(ot, p) setaddr(p, getaddr(ot)), setaddr(ot, p)
#define addPrims(ot, p0, p1) setaddr(p1, getaddr(ot)), setaddr(ot, p0)
#define catPrim(p0, p1) setaddr(p0, p1)
#define termPrim(p) setaddr(p, 0xffffffff)
#define isendprim(p) ((((P_TAG *)(p))->addr) == 0xffffff)

#define setSemiTrans(p, abe) ((abe) ? setcode(p, getcode(p) | 0x02) : setcode(p, getcode(p) & ~0x02))
#define setShadeTex(p, tge) ((tge) ? setcode(p, getcode(p) | 0x01) : setcode(p, getcode(p) & ~0x01))

#define _get_mode(dfe, dtd, tpage)                                                  \
    ((0xe1000000) | ((dtd) ? 0x0200 : 0) | ((dfe) ? 0x0400 : 0) | ((tpage) & 0x9ff))
#define setDrawTPage(p, dfe, dtd, tpage)                                            \
    setlen(p, 1), ((u_long *)(p))[1] = _get_mode(dfe, dtd, tpage)

#define setPolyF3(p) setlen(p, 4), setcode(p, 0x20)
#define setPolyFT3(p) setlen(p, 7), setcode(p, 0x24)
#define setPolyG3(p) setlen(p, 6), setcode(p, 0x30)
#define setPolyGT3(p) setlen(p, 9), setcode(p, 0x34)
#define setPolyF4(p) setlen(p, 5), setcode(p, 0x28)
#define setPolyFT4(p) setlen(p, 9), setcode(p, 0x2c)
#define setPolyG4(p) setlen(p, 8), setcode(p, 0x38)
#define setPolyGT4(p) setlen(p, 12), setcode(p, 0x3c)
#define setSprt8(p) setlen(p, 3), setcode(p, 0x74)
#define setSprt16(p) setlen(p, 3), setcode(p, 0x7c)
#define setSprt(p) setlen(p, 4), setcode(p, 0x64)
#define setTile1(p) setlen(p, 2), setcode(p, 0x68)
#define setTile8(p) setlen(p, 2), setcode(p, 0x70)
#define setTile16(p) setlen(p, 2), setcode(p, 0x78)
#define setTile(p) setlen(p, 3), setcode(p, 0x60)
#define setLineF2(p) setlen(p, 3), setcode(p, 0x40)
#define setLineG2(p) setlen(p, 4), setcode(p, 0x50)
#define setLineF3(p) setlen(p, 5), setcode(p, 0x48), (p)->pad = 0x55555555
#define setLineF4(p) setlen(p, 6), setcode(p, 0x4c), (p)->pad = 0x55555555

int ResetGraph(int mode);
int SetGraphDebug(int level);
int DrawSync(int mode);
void DrawOTag(u_long *ot);
void DrawOTagEnv(u_long *ot, DRAWENV *env);
void DrawPrim(void *p);
u_long *ClearOTag(u_long *ot, int n);
u_long *ClearOTagR(u_long *ot, int n);
DRAWENV *SetDefDrawEnv(DRAWENV *env, int x, int y, int w, int h);
DISPENV *SetDefDispEnv(DISPENV *env, int x, int y, int w, int h);
DRAWENV *PutDrawEnv(DRAWENV *env);
DISPENV *PutDispEnv(DISPENV *env);
void SetDispMask(int mask);
int LoadImage(RECT *rect, u_long *p);
int StoreImage(RECT *rect, u_long *p);
int MoveImage(RECT *rect, int x, int y);
int ClearImage(RECT *rect, u_char r, u_char g, u_char b);
int ClearImage2(RECT *rect, u_char r, u_char g, u_char b);
u_short GetTPage(int tp, int abr, int x, int y);
u_short GetClut(int x, int y);
void SetDrawMode(DR_MODE *p, int dfe, int dtd, int tpage, RECT *tw);
void SetDrawTPage(DR_TPAGE *p, int dfe, int dtd, int tpage);
void SetDrawArea(DR_AREA *p, RECT *r);
void SetDrawOffset(DR_OFFSET *p, u_short *ofs);
void SetDrawLoad(DR_LOAD *p, RECT *rect);
void SetDrawMove(DR_MOVE *p, RECT *rect, int x, int y);
void SetDrawEnv(DR_ENV *dr_env, DRAWENV *env);
void SetTexWindow(DR_TWIN *p, RECT *tw);
void SetSemiTrans(void *p, int abe);
void SetShadeTex(void *p, int tge);
void SetPolyF3(POLY_F3 *p);
void SetPolyF4(POLY_F4 *p);
void SetPolyFT3(POLY_FT3 *p);
void SetPolyFT4(POLY_FT4 *p);
void SetPolyG3(POLY_G3 *p);
void SetPolyG4(POLY_G4 *p);
void SetPolyGT3(POLY_GT3 *p);
void SetPolyGT4(POLY_GT4 *p);
void SetSprt(SPRT *p);
void SetSprt8(SPRT_8 *p);
void SetSprt16(SPRT_16 *p);
void SetTile(TILE *p);
void SetTile1(TILE_1 *p);
void SetLineF2(LINE_F2 *p);
void SetLineG2(LINE_G2 *p);
void AddPrim(void *ot, void *p);
void TermPrim(void *p);

int gpu_gl_init(void);
void gpu_gl_shutdown(void);
/* Clears the OpenGL frame now and makes (r, g, b) the color later frames start from. */
void gpu_clear_screen(u_char r, u_char g, u_char b);
/* Clears the OpenGL frame to the current background color. */
void gpu_clear_frame(void);
void gpu_debug_reset_clear(void);
/* Reads the back-buffer pixel at PS1 screen position (x, y). */
int gpu_debug_read_pixel(int x, int y, unsigned char rgb[3]);
/* PutDrawEnv: the draw-mode page, and a background clear when isbg is set. */
void gpu_put_draw_env(u_short tpage, u_char isbg, u_char r, u_char g, u_char b);
/* Draws the host green quad. Does not count as a game ordering table. */
void gpu_draw_demo(void);
/* 1 if DrawOTag ran since the previous call. */
int gpu_take_game_drew(void);
/* Game DrawOTag calls so far, and the packets of a drawn type they walked. */
void gpu_debug_game_stats(int *calls, int *packets);
/* Walks like DrawOTag without drawing; returns the packets of a drawn type. */
int gpu_debug_walk(u_long *ot);
int gpu_debug_last_steps(void);

#endif
