#ifndef LIBSPU_H
#define LIBSPU_H

#include <sys/types.h>

#define SPU_00CH (1 << 0)
#define SPU_01CH (1 << 1)
#define SPU_02CH (1 << 2)
#define SPU_03CH (1 << 3)
#define SPU_04CH (1 << 4)
#define SPU_05CH (1 << 5)
#define SPU_06CH (1 << 6)
#define SPU_07CH (1 << 7)
#define SPU_08CH (1 << 8)
#define SPU_09CH (1 << 9)
#define SPU_10CH (1 << 10)
#define SPU_11CH (1 << 11)
#define SPU_12CH (1 << 12)
#define SPU_13CH (1 << 13)
#define SPU_14CH (1 << 14)
#define SPU_15CH (1 << 15)
#define SPU_16CH (1 << 16)
#define SPU_17CH (1 << 17)
#define SPU_18CH (1 << 18)
#define SPU_19CH (1 << 19)
#define SPU_20CH (1 << 20)
#define SPU_21CH (1 << 21)
#define SPU_22CH (1 << 22)
#define SPU_23CH (1 << 23)
#define SPU_ALLCH 0xffffff

#define SPU_OFF 0
#define SPU_ON 1
#define SPU_BIT 8

#define SPU_COMMON_MVOLL (0x01 << 0)
#define SPU_COMMON_MVOLR (0x01 << 1)
#define SPU_COMMON_MVOLMODEL (0x01 << 2)
#define SPU_COMMON_MVOLMODER (0x01 << 3)
#define SPU_COMMON_RVOLL (0x01 << 4)
#define SPU_COMMON_RVOLR (0x01 << 5)
#define SPU_COMMON_CDVOLL (0x01 << 6)
#define SPU_COMMON_CDVOLR (0x01 << 7)
#define SPU_COMMON_CDREV (0x01 << 8)
#define SPU_COMMON_CDMIX (0x01 << 9)
#define SPU_COMMON_EXTVOLL (0x01 << 10)
#define SPU_COMMON_EXTVOLR (0x01 << 11)
#define SPU_COMMON_EXTREV (0x01 << 12)
#define SPU_COMMON_EXTMIX (0x01 << 13)

typedef struct {
    short left;
    short right;
} SpuVolume;

typedef struct {
    SpuVolume volume;
    long reverb;
    long mix;
} SpuExtAttr;

typedef struct {
    u_long mask;
    SpuVolume mvol;
    SpuVolume mvolmode;
    SpuVolume mvolx;
    SpuExtAttr cd;
    SpuExtAttr ext;
} SpuCommonAttr;

void SpuInit(void);
void SpuQuit(void);
void SpuSetCommonAttr(SpuCommonAttr *attr);
long SpuSetReverb(long on_off);
u_long SpuSetKey(long on_off, u_long voice_bit);
long SpuSetTransferMode(long mode);
u_long SpuSetTransferStartAddr(u_long addr);
u_long SpuWrite(u_char *addr, u_long size);
long SpuIsTransferCompleted(long flag);
long SpuSetMute(long on_off);
long SpuGetKeyStatus(u_long voice_bit);
void SpuGetAllKeysStatus(char *status);
long SpuSetIRQ(long on_off);

#endif
