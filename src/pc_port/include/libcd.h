#ifndef LIBCD_H
#define LIBCD_H

#include <sys/types.h>

#define CdlNop 0x00
#define CdlGetstat 0x01
#define CdlSetloc 0x02
#define CdlPlay 0x03
#define CdlForward 0x04
#define CdlBackward 0x05
#define CdlReadN 0x06
#define CdlStandby 0x07
#define CdlStop 0x08
#define CdlPause 0x09
#define CdlMute 0x0b
#define CdlDemute 0x0c
#define CdlSetfilter 0x0d
#define CdlSetmode 0x0e
#define CdlGetparam 0x0f
#define CdlGetlocL 0x10
#define CdlGetlocP 0x11
#define CdlGetTN 0x13
#define CdlGetTD 0x14
#define CdlSeekL 0x15
#define CdlSeekP 0x16
#define CdlReadS 0x1b

#define CdlNoIntr 0x00
#define CdlDataReady 0x01
#define CdlComplete 0x02
#define CdlAcknowledge 0x03
#define CdlDataEnd 0x04
#define CdlDiskError 0x05

#define CdlModeStream 0x100
#define CdlModeStream2 0x120
#define CdlModeSpeed 0x80
#define CdlModeRT 0x40
#define CdlModeSize1 0x20
#define CdlModeSize0 0x10
#define CdlModeSF 0x08
#define CdlModeRept 0x04
#define CdlModeAP 0x02
#define CdlModeDA 0x01

typedef struct {
    u_char minute;
    u_char second;
    u_char sector;
    u_char track;
} CdlLOC;

typedef struct {
    CdlLOC pos;
    u_long size;
    char name[16];
} CdlFILE;

typedef struct {
    u_char file;
    u_char chan;
    u_short pad;
} CdlFILTER;

typedef void (*CdlCB)(u_char, u_char *);

int CdInit(void);
int CdControl(u_char com, u_char *param, u_char *result);
int CdControlB(u_char com, u_char *param, u_char *result);
int CdControlF(u_char com, u_char *param);
int CdSync(int mode, u_char *result);
int CdReady(int mode, u_char *result);
CdlCB CdSyncCallback(CdlCB func);
CdlCB CdReadyCallback(CdlCB func);
int CdRead(int sectors, u_long *buf, int mode);
int CdReadSync(int mode, u_char *result);
CdlCB CdReadCallback(CdlCB func);
void CdReadBreak(void);
void CdFlush(void);
CdlFILE *CdSearchFile(CdlFILE *fp, char *name);
CdlLOC *CdIntToPos(int i, CdlLOC *p);
int CdPosToInt(CdlLOC *p);
int CdMix(void *vol);

#endif
