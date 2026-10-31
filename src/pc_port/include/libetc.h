#ifndef LIBETC_H
#define LIBETC_H

#include <sys/types.h>

#define MODE_NTSC 0
#define MODE_PAL 1

int VSync(int mode);
int VSyncCallback(void (*f)(void));
void ResetCallback(void);
int SetVideoMode(long mode);
long GetVideoMode(void);
int CheckCallback(void);
void PadInit(int mode);
u_long PadRead(int id);
void PadStop(void);

#endif
