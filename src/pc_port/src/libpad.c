/* libpad: one standard pad on port 1. The game registers two receive buffers with
 * PadInitDirect; each vblank the host writes port 1's from the PC's input. */

#include <libpad.h>
#include <event_guard.h>
#include <host_input.h>

#include <stddef.h>

#define PAD_PORT1 0x00
#define PAD_TYPE_DIGITAL_ID 0x41
#define PAD_TYPE_ANALOG_ID 0x73
#define PAD_STATUS_NONE 0xFF

static u_char *pad_buffer[2];
static int pad_started;
static int pad_analog;

void PadInitDirect(u_char *pad1, u_char *pad2) {
    pad_buffer[0] = pad1;
    pad_buffer[1] = pad2;
    if (pad1 != NULL) {
        pad1[0] = PAD_STATUS_NONE;
    }
    if (pad2 != NULL) {
        pad2[0] = PAD_STATUS_NONE;
    }
}

void PadStartCom(void) {
    pad_started = 1;
}

int PadGetState(int port) {
    event_guard_apply();
    if (port == PAD_PORT1 && pad_started && pad_buffer[0] != NULL) {
        return PadStateStable;
    }
    return PadStateDiscon;
}

int PadInfoMode(int port, int term, int offs) {
    (void)offs;
    if (port == PAD_PORT1 && term == InfoModeCurExID && pad_analog) {
        return PAD_TYPE_ANALOG_ID;
    }
    return 0;
}

int PadSetAct(int port, u_char *table, int len) {
    (void)port;
    (void)table;
    (void)len;
    return 0;
}

int PadSetActAlign(int port, u_char *table) {
    (void)table;
    return port == PAD_PORT1 ? 1 : 0;
}

void pad_write_port1(uint16_t held, int analog, const uint8_t sticks[4]) {
    u_char *b;
    int i;

    pad_analog = analog;
    b = pad_buffer[0];
    if (b == NULL) {
        return;
    }
    b[0] = 0x00;
    b[1] = analog ? PAD_TYPE_ANALOG_ID : PAD_TYPE_DIGITAL_ID;
    b[2] = (u_char)~(held >> 8);
    b[3] = (u_char)~(held & 0xFF);
    for (i = 0; i < 4; i++) {
        b[4 + i] = sticks[i];
    }
    if (pad_buffer[1] != NULL) {
        pad_buffer[1][0] = PAD_STATUS_NONE;
    }
}

int pad_is_analog(void) {
    return pad_analog;
}

void pad_debug_reset(void) {
    pad_buffer[0] = NULL;
    pad_buffer[1] = NULL;
    pad_started = 0;
    pad_analog = 0;
}
