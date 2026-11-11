/* Keeps scripted events from taking the pad while a scene is being tested.
 * The game's own debug bits make event_request_step and event_triggers_run return
 * before a script runs. The lock flags are cleared in the host, on the simulated
 * farm state, so the next player update still sees a free character. */

#include <event_guard.h>

#include "event_flags.h"
#include "farm_state.h"

#include <SDL.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int events_disabled(void) {
    const char *env;

    env = getenv("HMBTN_DISABLE_EVENTS");
    return env != NULL && strcmp(env, "1") == 0;
}

void event_guard_apply(void) {
    static int logged;

    if (!events_disabled()) {
        return;
    }
    game_state.debugFlags |= DEBUG_FLAG_NO_EVENTS | DEBUG_FLAG_NO_EVENT_TRIGGERS;
    game_state.player.stateFlags &=
        ~(PLAYER_FLAG_SCRIPTED | PLAYER_FLAG_NO_INPUT | PLAYER_FLAG_SCENE_CHANGE | PLAYER_FLAG_AUTO_WALK);
    game_state.globalFlags &=
        ~(GLOBAL_FLAG_PAUSED | GLOBAL_FLAG_TRANSITION | GLOBAL_FLAG_TRANSITION_BEGIN | GLOBAL_FLAG_CAMERA_BUSY);
    game_state.player.scriptHeld = 0;
    game_state.player.scriptPressed = 0;
    event_flag_set(EVENT_FLAG_EVENT_ACTIVE, 0);
    event_flag_set(EVENT_FLAG_REQUEST_BUSY, 0);
    event_flag_set(EVENT_FLAG_MORNING_MESSAGE, 0);
    if (!logged) {
        SDL_Log("events: dispatcher skipped, player control left free");
        logged = 1;
    }
}

static void put_env(const char *value) {
    static char slot[64];

    snprintf(slot, sizeof slot, "HMBTN_DISABLE_EVENTS=%s", value == NULL ? "" : value);
    _putenv(slot);
}

static int check(int ok, const char *what) {
    SDL_Log("event guard check %s: %s", ok ? "ok" : "FAILED", what);
    return ok ? 0 : 1;
}

int event_guard_self_check(void) {
    u32 saved_debug;
    u32 saved_global;
    u32 saved_state;
    s32 saved_held;
    s32 saved_pressed;
    u32 saved_flags0;
    u32 saved_flags5;
    const char *env;
    char saved_env[64];
    int failed;

    saved_debug = game_state.debugFlags;
    saved_global = game_state.globalFlags;
    saved_state = game_state.player.stateFlags;
    saved_held = game_state.player.scriptHeld;
    saved_pressed = game_state.player.scriptPressed;
    saved_flags0 = game_state.eventFlags[0];
    saved_flags5 = game_state.eventFlags[5];
    env = getenv("HMBTN_DISABLE_EVENTS");
    snprintf(saved_env, sizeof saved_env, "%s", env == NULL ? "" : env);

    failed = 0;
    game_state.debugFlags &= ~(DEBUG_FLAG_NO_EVENTS | DEBUG_FLAG_NO_EVENT_TRIGGERS);
    game_state.debugFlags |= DEBUG_FLAG_MUTEKI;
    game_state.player.stateFlags |=
        PLAYER_FLAG_SCRIPTED | PLAYER_FLAG_NO_INPUT | PLAYER_FLAG_SCENE_CHANGE | PLAYER_FLAG_AUTO_WALK;
    game_state.globalFlags |=
        GLOBAL_FLAG_PAUSED | GLOBAL_FLAG_TRANSITION | GLOBAL_FLAG_TRANSITION_BEGIN | GLOBAL_FLAG_CAMERA_BUSY;
    game_state.player.scriptHeld = 7;
    game_state.player.scriptPressed = 9;
    event_flag_set(EVENT_FLAG_EVENT_ACTIVE, 1);
    event_flag_set(EVENT_FLAG_REQUEST_BUSY, 1);
    event_flag_set(EVENT_FLAG_MORNING_MESSAGE, 1);

    put_env(NULL);
    event_guard_apply();
    failed |= check((game_state.debugFlags & (DEBUG_FLAG_NO_EVENTS | DEBUG_FLAG_NO_EVENT_TRIGGERS)) == 0 &&
                        (game_state.player.stateFlags & PLAYER_FLAG_SCRIPTED) != 0 &&
                        event_flag_get(EVENT_FLAG_EVENT_ACTIVE) == 1,
                    "an unset flag leaves the dispatcher and the lock alone");

    put_env("0");
    event_guard_apply();
    failed |= check(event_flag_get(EVENT_FLAG_REQUEST_BUSY) == 1, "a value other than 1 stays disarmed");

    put_env("1");
    event_guard_apply();
    failed |= check((game_state.debugFlags & (DEBUG_FLAG_NO_EVENTS | DEBUG_FLAG_NO_EVENT_TRIGGERS)) ==
                            (DEBUG_FLAG_NO_EVENTS | DEBUG_FLAG_NO_EVENT_TRIGGERS) &&
                        (game_state.debugFlags & DEBUG_FLAG_MUTEKI) != 0,
                    "the flag sets both event bits and keeps the others");
    failed |= check((game_state.player.stateFlags &
                     (PLAYER_FLAG_SCRIPTED | PLAYER_FLAG_NO_INPUT | PLAYER_FLAG_SCENE_CHANGE | PLAYER_FLAG_AUTO_WALK)) ==
                        0,
                    "the pad lock, the script, and the scene change are cleared");
    failed |= check((game_state.globalFlags & (GLOBAL_FLAG_PAUSED | GLOBAL_FLAG_TRANSITION | GLOBAL_FLAG_TRANSITION_BEGIN |
                                               GLOBAL_FLAG_CAMERA_BUSY)) == 0,
                    "pause, transition, and the camera lock are cleared");
    failed |= check(game_state.player.scriptHeld == 0 && game_state.player.scriptPressed == 0,
                    "scripted buttons are cleared");
    failed |= check(event_flag_get(EVENT_FLAG_EVENT_ACTIVE) == 0 && event_flag_get(EVENT_FLAG_REQUEST_BUSY) == 0 &&
                        event_flag_get(EVENT_FLAG_MORNING_MESSAGE) == 0,
                    "the active event, the busy request, and the morning message are cleared");

    game_state.debugFlags = saved_debug;
    game_state.globalFlags = saved_global;
    game_state.player.stateFlags = saved_state;
    game_state.player.scriptHeld = saved_held;
    game_state.player.scriptPressed = saved_pressed;
    game_state.eventFlags[0] = saved_flags0;
    game_state.eventFlags[5] = saved_flags5;
    put_env(saved_env[0] == '\0' ? NULL : saved_env);
    return failed == 0 ? 0 : 1;
}
