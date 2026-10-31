/* libmcx: no PocketStation is connected. */

#include <libmcx.h>

void McxStartCom(void) {
}

int McxSync(int mode, long *cmd, long *result) {
    (void)mode;
    if (cmd != 0) {
        *cmd = 0;
    }
    if (result != 0) {
        *result = McxErrNoCard;
    }
    return 1;
}

int McxGetApl(int port, long *aplno) {
    (void)port;
    (void)aplno;
    return 0;
}

int McxExecApl(int port, int aplno, long arg) {
    (void)port;
    (void)aplno;
    (void)arg;
    return 0;
}

int McxGetTime(int port, u_char *time) {
    (void)port;
    (void)time;
    return 0;
}

int McxGetMem(int port, u_char *data, u_int start, u_int len) {
    (void)port;
    (void)data;
    (void)start;
    (void)len;
    return 0;
}

int McxSetMem(int port, u_char *data, u_int start, u_int len) {
    (void)port;
    (void)data;
    (void)start;
    (void)len;
    return 0;
}

int McxShowTrans(int port, int dir, int timeout) {
    (void)port;
    (void)dir;
    (void)timeout;
    return 0;
}

int McxHideTrans(int port) {
    (void)port;
    return 0;
}

int McxCurrCtrl(int port, int sound, int infrared, int val) {
    (void)port;
    (void)sound;
    (void)infrared;
    (void)val;
    return 0;
}

int McxSetLED(int port, int val) {
    (void)port;
    (void)val;
    return 0;
}

int McxGetSerial(int port, u_long *serial) {
    (void)port;
    (void)serial;
    return 0;
}

int McxExecFlag(int port, int block, int flag) {
    (void)port;
    (void)block;
    (void)flag;
    return 0;
}

int McxAllInfo(int port, u_char *state) {
    (void)port;
    (void)state;
    return 0;
}

int McxCardType(int port) {
    (void)port;
    return 0;
}

int McxFlashAcs() {
    return 0;
}

int McxReadDev() {
    return 0;
}

int McxWriteDev() {
    return 0;
}
