/* Pad port and input mapping checks, run at startup when HMBTN_SELFTEST=1. They leave
 * the pad library as the game expects to find it: no buffers, not started. */

#include <host_input.h>
#include <libpad.h>

#include <SDL.h>

#include <string.h>

static int check(int ok, const char *what) {
    SDL_Log("input check %s: %s", ok ? "ok" : "FAILED", what);
    return ok ? 0 : 1;
}

static uint16_t keys_for(SDL_Scancode a, SDL_Scancode b) {
    uint8_t keys[SDL_NUM_SCANCODES];

    memset(keys, 0, sizeof(keys));
    keys[a] = 1;
    if (b != SDL_SCANCODE_UNKNOWN) {
        keys[b] = 1;
    }
    return host_input_keys_to_buttons(keys);
}

static int check_port(void) {
    static const uint8_t centered[4] = {0x80, 0x80, 0x80, 0x80};
    static const uint8_t sticks[4] = {0x10, 0x20, 0x30, 0x40};
    u_char buf1[34];
    u_char buf2[34];
    u_char untouched[34];
    int failed;

    pad_debug_reset();
    memset(untouched, 0xAA, sizeof(untouched));
    pad_write_port1(HOST_PAD_START, 0, centered);
    failed = check(PadGetState(0x00) == PadStateDiscon, "before PadInitDirect and PadStartCom, port 0x00 is disconnected");

    memset(buf1, 0xAA, sizeof(buf1));
    memset(buf2, 0xAA, sizeof(buf2));
    PadInitDirect(buf1, buf2);
    failed |= check(PadGetState(0x00) == PadStateDiscon, "before PadStartCom, port 0x00 is disconnected");
    PadStartCom();
    failed |= check(PadGetState(0x00) == PadStateStable, "after PadStartCom, port 0x00 is stable");
    failed |= check(PadGetState(0x10) == PadStateDiscon, "port 0x10 is disconnected");
    failed |= check(PadSetActAlign(0x00, NULL) == 1, "PadSetActAlign(0x00) returns 1");

    pad_write_port1(0, 0, centered);
    failed |= check(buf1[0] == 0x00 && buf1[1] == 0x41 && buf1[2] == 0xFF && buf1[3] == 0xFF,
                    "no buttons: status 0x00, type 0x41, button bytes 0xFF");
    failed |= check(buf1[4] == 0x80 && buf1[7] == 0x80, "digital pad: stick bytes 0x80");
    failed |= check(PadInfoMode(0x00, InfoModeCurExID, 0) == 0, "digital pad: PadInfoMode is 0");

    pad_write_port1(HOST_PAD_START, 0, centered);
    failed |= check(buf1[2] == 0xF7 && buf1[3] == 0xFF, "Start held: byte 2 0xF7, byte 3 0xFF");
    pad_write_port1(HOST_PAD_CROSS, 0, centered);
    failed |= check(buf1[2] == 0xFF && buf1[3] == 0xBF, "Cross held: byte 3 0xBF");
    failed |= check((u_short)~((buf1[2] << 8) | buf1[3]) == HOST_PAD_CROSS, "the game's held decode gives PAD_CROSS");

    pad_write_port1(0, 1, sticks);
    failed |= check(buf1[1] == 0x73 && buf1[4] == 0x10 && buf1[5] == 0x20 && buf1[6] == 0x30 && buf1[7] == 0x40,
                    "analog pad: type 0x73 and its stick bytes");
    failed |= check(PadInfoMode(0x00, InfoModeCurExID, 0) == 0x73, "analog pad: PadInfoMode is 0x73");
    failed |= check(buf2[0] == 0xFF, "port 2 status stays 0xFF");
    failed |= check(memcmp(buf1 + 8, untouched + 8, sizeof(buf1) - 8) == 0, "only the first 8 bytes are written");

    pad_debug_reset();
    return failed;
}

static int check_keys(void) {
    int failed;

    failed = check(keys_for(SDL_SCANCODE_RETURN, SDL_SCANCODE_UNKNOWN) == HOST_PAD_START, "Enter alone is exactly Start");
    failed |= check(keys_for(SDL_SCANCODE_UP, SDL_SCANCODE_X) == (HOST_PAD_UP | HOST_PAD_CROSS), "Up + X is Up and Cross");
    failed |= check(keys_for(SDL_SCANCODE_Z, SDL_SCANCODE_UNKNOWN) == HOST_PAD_CIRCLE, "Z is Circle");
    failed |= check(keys_for(SDL_SCANCODE_C, SDL_SCANCODE_UNKNOWN) == HOST_PAD_SQUARE, "C is Square");
    failed |= check(keys_for(SDL_SCANCODE_V, SDL_SCANCODE_UNKNOWN) == HOST_PAD_TRIANGLE, "V is Triangle");
    failed |= check(keys_for(SDL_SCANCODE_RSHIFT, SDL_SCANCODE_UNKNOWN) == HOST_PAD_SELECT, "Right Shift is Select");
    failed |= check(keys_for(SDL_SCANCODE_A, SDL_SCANCODE_UNKNOWN) == HOST_PAD_L1, "A is L1");
    failed |= check(keys_for(SDL_SCANCODE_S, SDL_SCANCODE_UNKNOWN) == HOST_PAD_R1, "S is R1");
    failed |= check(keys_for(SDL_SCANCODE_Q, SDL_SCANCODE_UNKNOWN) == HOST_PAD_L2, "Q is L2");
    failed |= check(keys_for(SDL_SCANCODE_W, SDL_SCANCODE_UNKNOWN) == HOST_PAD_R2, "W is R2");
    return failed;
}

static int check_controller(void) {
    uint8_t buttons[SDL_CONTROLLER_BUTTON_MAX];
    int16_t axes[SDL_CONTROLLER_AXIS_MAX];
    int failed;

    memset(buttons, 0, sizeof(buttons));
    memset(axes, 0, sizeof(axes));
    buttons[SDL_CONTROLLER_BUTTON_A] = 1;
    failed = check(host_input_controller_buttons(buttons, axes) == HOST_PAD_CROSS, "controller south is Cross");
    buttons[SDL_CONTROLLER_BUTTON_A] = 0;
    axes[SDL_CONTROLLER_AXIS_TRIGGERLEFT] = 20000;
    axes[SDL_CONTROLLER_AXIS_TRIGGERRIGHT] = 20000;
    failed |= check(host_input_controller_buttons(buttons, axes) == (HOST_PAD_L2 | HOST_PAD_R2),
                    "triggers at 20000 are L2 and R2");
    failed |= check(host_input_axis_byte(-32768) == 0 && host_input_axis_byte(32767) == 255,
                    "sticks at -32768 and 32767 are 0 and 255");
    return failed;
}

int input_self_check(void) {
    int failed;

    failed = check_port();
    failed |= check_keys();
    failed |= check_controller();
    return failed;
}
