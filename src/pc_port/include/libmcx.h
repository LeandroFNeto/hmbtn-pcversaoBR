#ifndef LIBMCX_H
#define LIBMCX_H

#include <sys/types.h>

#define McxFuncGetApl 1
#define McxFuncExecApl 2
#define McxFuncGetTime 3
#define McxFuncGetMem 4
#define McxFuncSetMem 5
#define McxFuncShowTrans 6
#define McxFuncHideTrans 7
#define McxFuncCurrCtrl 8
#define McxFuncSetLED 9
#define McxFuncGetSerial 10
#define McxFuncExecFlag 11
#define McxFuncAllInfo 12
#define McxFuncFlashAcs 13
#define McxFuncReadDev 14
#define McxFuncWriteDev 15
#define McxFuncGetUIFS 16
#define McxFuncSetUIFS 17
#define McxFuncSetTime 18
#define McxFuncCardType 19

#define McxErrSuccess 0
#define McxErrNoCard 1
#define McxErrInvalid 2
#define McxErrNewCard 3

void McxStartCom(void);
void McxStopCom(void);
int McxSync(int mode, long *cmd, long *result);
int McxGetApl(int port, long *aplno);
int McxExecApl(int port, int aplno, long arg);
int McxGetTime(int port, u_char *time);
int McxGetMem(int port, u_char *data, u_int start, u_int len);
int McxSetMem(int port, u_char *data, u_int start, u_int len);
int McxShowTrans(int port, int dir, int timeout);
int McxHideTrans(int port);
int McxCurrCtrl(int port, int sound, int infrared, int val);
int McxSetLED(int port, int val);
int McxGetSerial(int port, u_long *serial);
int McxExecFlag(int port, int block, int flag);
int McxAllInfo(int port, u_char *state);
int McxCardType(int port);

#endif
