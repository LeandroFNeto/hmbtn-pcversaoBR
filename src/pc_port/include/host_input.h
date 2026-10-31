#ifndef PC_PORT_HOST_INPUT_H
#define PC_PORT_HOST_INPUT_H

#include <stdint.h>

/* PC input to the PS1 pad. Buttons are active high here, in the bit positions of the
 * game's PadButton enum (pad.h): L2 0x0001 ... Start 0x0800, Up 0x1000 ... Left 0x8000.
 * libpad.c turns them into the active-low receive buffer. */

#define HOST_PAD_L2 0x0001
#define HOST_PAD_R2 0x0002
#define HOST_PAD_L1 0x0004
#define HOST_PAD_R1 0x0008
#define HOST_PAD_TRIANGLE 0x0010
#define HOST_PAD_CIRCLE 0x0020
#define HOST_PAD_CROSS 0x0040
#define HOST_PAD_SQUARE 0x0080
#define HOST_PAD_SELECT 0x0100
#define HOST_PAD_START 0x0800
#define HOST_PAD_UP 0x1000
#define HOST_PAD_RIGHT 0x2000
#define HOST_PAD_DOWN 0x4000
#define HOST_PAD_LEFT 0x8000

union SDL_Event;

/* Opens the first game controller; host_input_shutdown closes it. */
void host_input_init(void);
void host_input_shutdown(void);
/* Controller hot-plug events from the vblank's event loop. */
void host_input_handle_event(const union SDL_Event *event);
/* Reads keyboard and controller and writes the pad buffers. Once per vblank. */
void host_input_sample(void);

/* Pure parts, for checks. `keys` is indexed by SDL scancode. */
uint16_t host_input_keys_to_buttons(const uint8_t *keys);
/* `buttons` indexed by SDL_GameControllerButton (0 or 1), `axes` by SDL_GameControllerAxis. */
uint16_t host_input_controller_buttons(const uint8_t *buttons, const int16_t *axes);
uint8_t host_input_axis_byte(int16_t value);

/* libpad.c: write port 1's receive buffer (and port 2's status) from active-high buttons.
 * `sticks` is right X, right Y, left X, left Y. */
void pad_write_port1(uint16_t held, int analog, const uint8_t sticks[4]);
int pad_is_analog(void);
/* For checks: forget the registered buffers and the started flag. */
void pad_debug_reset(void);

#endif
