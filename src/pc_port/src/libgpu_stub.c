/* libgpu calls that do not draw. Environment setup fills the structs the way the SDK
 * does; transfers live in vram.c; font calls do nothing. */

#include <libgpu.h>

#include <string.h>

int ResetGraph(int mode) {
    (void)mode;
    return 0;
}

int SetGraphDebug(int level) {
    (void)level;
    return 0;
}

int DrawSync(int mode) {
    (void)mode;
    return 0;
}

void DrawOTagEnv(u_long *ot, DRAWENV *env) {
    (void)env;
    DrawOTag(ot);
}

void DrawPrim(void *p) {
    (void)p;
}

DRAWENV *SetDefDrawEnv(DRAWENV *env, int x, int y, int w, int h) {
    memset(env, 0, sizeof(*env));
    setRECT(&env->clip, x, y, w, h);
    env->ofs[0] = (short)x;
    env->ofs[1] = (short)y;
    env->tpage = 10;
    env->dtd = 1;
    env->dfe = 1;
    return env;
}

DISPENV *SetDefDispEnv(DISPENV *env, int x, int y, int w, int h) {
    memset(env, 0, sizeof(*env));
    setRECT(&env->disp, x, y, w, h);
    return env;
}

DRAWENV *PutDrawEnv(DRAWENV *env) {
    gpu_put_draw_env(env->tpage, env->isbg, env->r0, env->g0, env->b0);
    return env;
}

DISPENV *PutDispEnv(DISPENV *env) {
    return env;
}

void SetDispMask(int mask) {
    (void)mask;
}

u_short GetTPage(int tp, int abr, int x, int y) {
    return (u_short)getTPage(tp, abr, x, y);
}

u_short GetClut(int x, int y) {
    return (u_short)getClut(x, y);
}

void SetDrawMode(DR_MODE *p, int dfe, int dtd, int tpage, RECT *tw) {
    setlen(p, 2);
    p->code[0] = _get_mode(dfe, dtd, tpage);
    p->code[1] = tw != NULL ? 0xe2000000u : 0;
}

void SetDrawTPage(DR_TPAGE *p, int dfe, int dtd, int tpage) {
    setDrawTPage(p, dfe, dtd, tpage);
}

void SetDrawArea(DR_AREA *p, RECT *r) {
    setlen(p, 2);
    p->code[0] = 0xe3000000u | ((u_long)(r->y & 0x3ff) << 10) | (u_long)(r->x & 0x3ff);
    p->code[1] = 0xe4000000u | ((u_long)((r->y + r->h - 1) & 0x3ff) << 10) |
                 (u_long)((r->x + r->w - 1) & 0x3ff);
}

void SetDrawOffset(DR_OFFSET *p, u_short *ofs) {
    setlen(p, 2);
    p->code[0] = 0xe5000000u | ((u_long)(ofs[1] & 0x7ff) << 11) | (u_long)(ofs[0] & 0x7ff);
    p->code[1] = 0;
}

void SetDrawLoad(DR_LOAD *p, RECT *rect) {
    setlen(p, 3 + ((rect->w * rect->h + 1) / 2));
    p->code[0] = 0xa0000000u;
    p->code[1] = ((u_long)(u_short)rect->y << 16) | (u_short)rect->x;
    p->code[2] = ((u_long)(u_short)rect->h << 16) | (u_short)rect->w;
}

void SetDrawMove(DR_MOVE *p, RECT *rect, int x, int y) {
    setlen(p, 5);
    p->code[0] = 0x01000000u;
    p->code[1] = 0x80000000u;
    p->code[2] = ((u_long)(u_short)rect->y << 16) | (u_short)rect->x;
    p->code[3] = ((u_long)(u_short)y << 16) | (u_short)x;
    p->code[4] = ((u_long)(u_short)rect->h << 16) | (u_short)rect->w;
}

void SetDrawEnv(DR_ENV *dr_env, DRAWENV *env) {
    (void)env;
    setlen(dr_env, 0);
}

void SetTexWindow(DR_TWIN *p, RECT *tw) {
    (void)tw;
    setlen(p, 2);
    p->code[0] = 0xe2000000u;
    p->code[1] = 0;
}

void SetSemiTrans(void *p, int abe) {
    setSemiTrans(p, abe);
}

void SetShadeTex(void *p, int tge) {
    setShadeTex(p, tge);
}

void SetPolyF3(POLY_F3 *p) {
    setPolyF3(p);
}

void SetPolyF4(POLY_F4 *p) {
    setPolyF4(p);
}

void SetPolyFT3(POLY_FT3 *p) {
    setPolyFT3(p);
}

void SetPolyFT4(POLY_FT4 *p) {
    setPolyFT4(p);
}

void SetPolyG3(POLY_G3 *p) {
    setPolyG3(p);
}

void SetPolyG4(POLY_G4 *p) {
    setPolyG4(p);
}

void SetPolyGT3(POLY_GT3 *p) {
    setPolyGT3(p);
}

void SetPolyGT4(POLY_GT4 *p) {
    setPolyGT4(p);
}

void SetSprt(SPRT *p) {
    setSprt(p);
}

void SetSprt8(SPRT_8 *p) {
    setSprt8(p);
}

void SetSprt16(SPRT_16 *p) {
    setSprt16(p);
}

void SetTile(TILE *p) {
    setTile(p);
}

void SetTile1(TILE_1 *p) {
    setTile1(p);
}

void SetLineF2(LINE_F2 *p) {
    setLineF2(p);
}

void SetLineG2(LINE_G2 *p) {
    setLineG2(p);
}

void AddPrim(void *ot, void *p) {
    addPrim(ot, p);
}

void TermPrim(void *p) {
    termPrim(p);
}

/* Debug and kanji fonts: the game prints through them; the host shows nothing. */
void FntLoad(int tx, int ty) {
    (void)tx;
    (void)ty;
}

int FntOpen(int x, int y, int w, int h, int isbg, int n) {
    (void)x;
    (void)y;
    (void)w;
    (void)h;
    (void)isbg;
    (void)n;
    return 0;
}

u_long *FntFlush(int id) {
    (void)id;
    return NULL;
}

int FntPrint() {
    return 0;
}

void SetDumpFnt(int id) {
    (void)id;
}

int KanjiFntOpen() {
    return 0;
}

void KanjiFntClose(void) {
}

u_long *KanjiFntFlush(int id) {
    (void)id;
    return NULL;
}

int KanjiFntPrint() {
    return 0;
}
