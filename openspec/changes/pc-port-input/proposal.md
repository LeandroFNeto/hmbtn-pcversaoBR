# Proposal

## Why

The host draws the title screen, but the game cannot leave it: the host `libpad` reports `PadStateDiscon` for both ports and never fills the pad buffers, so `pad_poll_ports` clears the input records every frame and `input_pad1->pressed` never has `PAD_START`. The title waits for Start (`title.c`, `TITLE_SEQ_IDLE` and `TITLE_SEQ_START_WAIT`) and, after 1,800 idle frames, falls into its attract demo.

## What Changes

- The host `libpad` emulates the pad library the way `src/main/input/pad.c` uses it. `PadInitDirect` keeps the game's two receive buffers. `PadGetState` reports port 1 as stable (`PadStateStable`) and port 2 as disconnected. `PadInfoMode`, `PadSetAct`, and `PadSetActAlign` answer as for a standard pad. Each vblank, the host writes port 1's buffer: status `0x00`, type digital (`0x41`) or analog (`0x73`), the two button bytes active low, and the stick bytes.
- The host reads the PC's input once per vblank, inside `VSync`, right after it polls window events: `SDL_GetKeyboardState` for the keyboard, and the first `SDL_GameController` if one is plugged in. Keyboard and controller buttons are combined.
- Keyboard layout: arrows for the d-pad, `X` ✕, `Z` ○, `C` □, `V` △, `Enter` Start, `Right Shift` Select, `A`/`S` L1/R1, `Q`/`W` L2/R2. Controller layout follows the PlayStation positions (south ✕, east ○, west □, north △), shoulders and triggers for L1/R1/L2/R2, Back for Select, Start for Start, and the left stick as the analog stick.
- Correction to the request: the game does not call `PadRead` or `PadInit`; it uses `libpad`'s direct buffers. Those are what the host fills.
- No change under `src/`.

## Capabilities

### New Capabilities

- `host-pad-port`: The host's pad library reports a connected standard pad on port 1 and keeps the game's receive buffer filled in the PS1 format.
- `host-input-mapping`: The host turns keyboard and game-controller state into the PS1 pad's buttons and stick once per vblank.

### Modified Capabilities

- None.

## Impact

- `src/pc_port/src/libpad.c` (rewritten), a new `src/pc_port/src/host_input.c` with `src/pc_port/include/host_input.h`, `libetc.c` (one call per vblank), `main_pc.c` (`SDL_INIT_GAMECONTROLLER`), `host_check.c` or a new check file.
- No new package: the game controller API is part of SDL2.
- Out of scope: port 2, rumble output (`PadSetAct` tables are accepted and ignored), remapping UI or a config file, multitap, and mouse.
