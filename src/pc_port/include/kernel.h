#ifndef KERNEL_H
#define KERNEL_H

#include <sys/types.h>

#define DescMask 0xff000000
#define DescTH DescMask
#define DescHW 0xf0000000
#define DescEV 0xf1000000
#define DescRC 0xf2000000
#define DescUEV 0xf3000000
#define DescSW 0xf4000000

#define HwVBLANK (DescHW | 0x01)
#define HwGPU (DescHW | 0x02)
#define HwCdRom (DescHW | 0x03)
#define HwDMAC (DescHW | 0x04)
#define HwRTC0 (DescHW | 0x05)
#define HwRTC1 (DescHW | 0x06)
#define HwRTC2 (DescHW | 0x07)
#define HwCNTL (DescHW | 0x08)
#define HwSPU (DescHW | 0x09)
#define HwPIO (DescHW | 0x0a)
#define HwSIO (DescHW | 0x0b)
#define HwCPU (DescHW | 0x10)
#define HwCARD (DescHW | 0x11)
#define HwCARD_0 (DescHW | 0x12)
#define HwCARD_1 (DescHW | 0x13)
#define SwCARD (DescSW | 0x01)
#define SwMATH (DescSW | 0x02)

#define EvSpIOE 0x0004
#define EvSpERROR 0x8000
#define EvSpTIMOUT 0x0100
#define EvSpNEW 0x2000
#define EvSpUNKNOWN 0x0800

#define EvMdINTR 0x1000
#define EvMdNOINTR 0x2000

#define O_RDONLY 0x0001
#define O_WRONLY 0x0002
#define O_RDWR 0x0003
#define O_NBLOCK 0x0004
#define O_APPEND 0x0100
#define O_CREAT 0x0200
#define O_TRUNC 0x0400
#define O_EXCL 0x0800

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

struct DIRENTRY {
    char name[20];
    long attr;
    long size;
    struct DIRENTRY *next;
    long head;
    char system[4];
};

#endif
