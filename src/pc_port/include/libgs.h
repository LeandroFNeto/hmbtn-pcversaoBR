#ifndef LIBGS_H
#define LIBGS_H

#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>

typedef unsigned char PACKET;

typedef struct {
    unsigned p : 24;
    unsigned char num : 8;
} GsOT_TAG;

typedef struct {
    unsigned long length;
    GsOT_TAG *org;
    unsigned long offset;
    unsigned long point;
    GsOT_TAG *tag;
} GsOT;

typedef struct {
    long vx, vy, vz;
    unsigned char r, g, b;
} GsF_LIGHT;

extern MATRIX GsIDMATRIX;
extern u_short HWD0, VWD0;

void GsInitGraph(unsigned short x, unsigned short y, unsigned short intmode,
                 unsigned short dith, unsigned short varmmode);
void GsDefDispBuff(unsigned short x0, unsigned short y0, unsigned short x1, unsigned short y1);
void GsSetOffset(long x, long y);
void GsSetDrawBuffOffset(void);
void GsInit3D(void);
void GsSetWorkBase(PACKET *base);
int GsSetFlatLight(int id, GsF_LIGHT *lt);
void GsSetLightMode(int mode);
void GsSetAmbient(long r, long g, long b);

#endif
