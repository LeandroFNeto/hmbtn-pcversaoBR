#ifndef LIBPAD_H
#define LIBPAD_H

#include <sys/types.h>

#define PadStateDiscon 0
#define PadStateFindPad 1
#define PadStateFindCTP1 2
#define PadStateFindCTP2 3
#define PadStateReqInfo 4
#define PadStateExecCmd 5
#define PadStateStable 6

#define InfoModeCurID 1
#define InfoModeCurExID 2
#define InfoModeCurExOffs 3
#define InfoModeIdTable 4

#define InfoActFunc 1
#define InfoActSub 2
#define InfoActSize 3
#define InfoActCurr 4
#define InfoActSign 5

#define PadMaxCurr 60
#define PadCurrCTP1 10

void PadInitDirect(u_char *pad1, u_char *pad2);
void PadInitMtap(u_char *pad1, u_char *pad2);
void PadStartCom(void);
void PadStopCom(void);
int PadGetState(int port);
int PadInfoMode(int port, int term, int offs);
int PadInfoAct(int port, int acno, int term);
int PadInfoComb(int port, int listno, int offs);
int PadSetAct(int port, u_char *table, int len);
int PadSetActAlign(int port, u_char *table);
int PadSetMainMode(int port, int offs, int lock);
void PadChkVsync(void);

#endif
