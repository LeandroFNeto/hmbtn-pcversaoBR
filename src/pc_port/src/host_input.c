/* Keyboard and game controller to the PS1 pad, sampled once per vblank. */

#include <host_input.h>
#include <spu_core.h>

#include <SDL.h>

#include <stddef.h>

#define TRIGGER_THRESHOLD 16384

static SDL_GameController *controller;
static SDL_JoystickID controller_id = -1;
static uint16_t last_held;

typedef struct {
    SDL_Scancode key;
    uint16_t button;
} KeyBinding;

static const KeyBinding key_bindings[] = {
    {SDL_SCANCODE_UP, HOST_PAD_UP},
    {SDL_SCANCODE_DOWN, HOST_PAD_DOWN},
    {SDL_SCANCODE_LEFT, HOST_PAD_LEFT},
    {SDL_SCANCODE_RIGHT, HOST_PAD_RIGHT},
    {SDL_SCANCODE_X, HOST_PAD_CROSS},
    {SDL_SCANCODE_Z, HOST_PAD_CIRCLE},
    {SDL_SCANCODE_C, HOST_PAD_SQUARE},
    {SDL_SCANCODE_V, HOST_PAD_TRIANGLE},
    {SDL_SCANCODE_RETURN, HOST_PAD_START},
    {SDL_SCANCODE_KP_ENTER, HOST_PAD_START},
    {SDL_SCANCODE_RSHIFT, HOST_PAD_SELECT},
    {SDL_SCANCODE_A, HOST_PAD_L1},
    {SDL_SCANCODE_S, HOST_PAD_R1},
    {SDL_SCANCODE_Q, HOST_PAD_L2},
    {SDL_SCANCODE_W, HOST_PAD_R2},
};

typedef struct {
    SDL_GameControllerButton button;
    uint16_t pad;
} ButtonBinding;

static const ButtonBinding button_bindings[] = {
    {SDL_CONTROLLER_BUTTON_A, HOST_PAD_CROSS},
    {SDL_CONTROLLER_BUTTON_B, HOST_PAD_CIRCLE},
    {SDL_CONTROLLER_BUTTON_X, HOST_PAD_SQUARE},
    {SDL_CONTROLLER_BUTTON_Y, HOST_PAD_TRIANGLE},
    {SDL_CONTROLLER_BUTTON_LEFTSHOULDER, HOST_PAD_L1},
    {SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, HOST_PAD_R1},
    {SDL_CONTROLLER_BUTTON_BACK, HOST_PAD_SELECT},
    {SDL_CONTROLLER_BUTTON_START, HOST_PAD_START},
    {SDL_CONTROLLER_BUTTON_DPAD_UP, HOST_PAD_UP},
    {SDL_CONTROLLER_BUTTON_DPAD_DOWN, HOST_PAD_DOWN},
    {SDL_CONTROLLER_BUTTON_DPAD_LEFT, HOST_PAD_LEFT},
    {SDL_CONTROLLER_BUTTON_DPAD_RIGHT, HOST_PAD_RIGHT},
};

uint16_t host_input_keys_to_buttons(const uint8_t *keys) {
    uint16_t buttons;
    size_t i;

    buttons = 0;
    for (i = 0; i < sizeof(key_bindings) / sizeof(key_bindings[0]); i++) {
        if (keys[key_bindings[i].key]) {
            buttons |= key_bindings[i].button;
        }
    }
    return buttons;
}

uint16_t host_input_controller_buttons(const uint8_t *buttons, const int16_t *axes) {
    uint16_t pad;
    size_t i;

    pad = 0;
    for (i = 0; i < sizeof(button_bindings) / sizeof(button_bindings[0]); i++) {
        if (buttons[button_bindings[i].button]) {
            pad |= button_bindings[i].pad;
        }
    }
    if (axes[SDL_CONTROLLER_AXIS_TRIGGERLEFT] > TRIGGER_THRESHOLD) {
        pad |= HOST_PAD_L2;
    }
    if (axes[SDL_CONTROLLER_AXIS_TRIGGERRIGHT] > TRIGGER_THRESHOLD) {
        pad |= HOST_PAD_R2;
    }
    return pad;
}

uint8_t host_input_axis_byte(int16_t value) {
    return (uint8_t)(((int)value + 32768) >> 8);
}

static void open_controller(int index) {
    if (controller != NULL || !SDL_IsGameController(index)) {
        return;
    }
    controller = SDL_GameControllerOpen(index);
    if (controller != NULL) {
        controller_id = SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(controller));
        SDL_Log("input: controller %s", SDL_GameControllerName(controller));
    }
}

static void close_controller(void) {
    if (controller != NULL) {
        SDL_GameControllerClose(controller);
        controller = NULL;
        controller_id = -1;
        SDL_Log("input: controller closed");
    }
}

void host_input_init(void) {
    int i;

    for (i = 0; i < SDL_NumJoysticks() && controller == NULL; i++) {
        open_controller(i);
    }
}

void host_input_shutdown(void) {
    close_controller();
}

void host_input_handle_event(const SDL_Event *event) {
    if (event->type == SDL_CONTROLLERDEVICEADDED) {
        open_controller(event->cdevice.which);
    } else if (event->type == SDL_CONTROLLERDEVICEREMOVED && event->cdevice.which == controller_id) {
        close_controller();
        host_input_init();
    }
}

void host_input_sample(void) {
    uint8_t buttons[SDL_CONTROLLER_BUTTON_MAX];
    int16_t axes[SDL_CONTROLLER_AXIS_MAX];
    uint8_t sticks[4] = {0x80, 0x80, 0x80, 0x80};
    uint16_t held;
    int analog;
    int i;

    held = host_input_keys_to_buttons(SDL_GetKeyboardState(NULL));
    analog = 0;
    if (controller != NULL) {
        for (i = 0; i < SDL_CONTROLLER_BUTTON_MAX; i++) {
            buttons[i] = SDL_GameControllerGetButton(controller, (SDL_GameControllerButton)i);
        }
        for (i = 0; i < SDL_CONTROLLER_AXIS_MAX; i++) {
            axes[i] = SDL_GameControllerGetAxis(controller, (SDL_GameControllerAxis)i);
        }
        held |= host_input_controller_buttons(buttons, axes);
        sticks[0] = host_input_axis_byte(axes[SDL_CONTROLLER_AXIS_RIGHTX]);
        sticks[1] = host_input_axis_byte(axes[SDL_CONTROLLER_AXIS_RIGHTY]);
        sticks[2] = host_input_axis_byte(axes[SDL_CONTROLLER_AXIS_LEFTX]);
        sticks[3] = host_input_axis_byte(axes[SDL_CONTROLLER_AXIS_LEFTY]);
        analog = 1;
    }
    if (held != last_held && spu_debug_tracing()) {
        SDL_Log("input: buttons 0x%04X at dump frame %u", held, (unsigned)spu_debug_dump_frames());
    }
    last_held = held;
    pad_write_port1(held, analog, sticks);
}
