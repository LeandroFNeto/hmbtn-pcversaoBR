# Design

## Context

See proposal.md for why. How the game reads its pad, from `src/main/input/pad.c` and `pad.h`:

- `pad_init` → `pad_start_ports`: `PadInitDirect(&pad_buffers[0], &pad_buffers[1])`, `PadSetAct(0x00, …, 8)`, `PadSetAct(0x10, …, 8)`, `PadStartCom()`, then `ChangeClearPAD(0)`.
- `pad_poll_ports`, once per game frame from `main_run_frame`, asks `PadGetState(port)` for ports `0x00` and `0x10`. Disconnected or searching clears the record. Stable: on first sight it calls `PadInfoMode(port, InfoModeCurExID, 0)` and `PadSetActAlign`; then it reads the buffer.
- `pad_read_buttons` returns an error unless `status == 0` and the type nibble (`type >> 4`) is 4 (digital) or 7 (analog), and otherwise `(buttons << 8) | buttonsHigh` from bytes 2 and 3. `held = ~that`, so the bytes are active low. The `PadButton` enum fixes the bit positions (Start `0x0800`, Up `0x1000`, ✕ `0x0040`, …).
- For an analog pad, `pad_dpad_with_stick` folds the left stick (bytes 6 and 7) into the d-pad when no d-pad bit is pressed, and `pad_read_analog` turns it into a tilt.
- The title advances on `input_pad1->pressed & PAD_START` (and `PAD_CROSS` in its first wait). `pressed` is an edge: the button must be up in one poll and down in the next.
- The host already polls SDL events in `host_vblank` (`libetc.c`). `SDL_Init` uses `SDL_INIT_VIDEO | SDL_INIT_EVENTS`.

## Goals / Non-Goals

**Goals:**

- The game's own `pad.c` runs unchanged and reads the PC's input.
- Keyboard always works; a game controller works when plugged in, including hot-plug.
- Input is latched at the vblank, as the PS1's pad interrupt does.

**Non-Goals:**

- Rumble, port 2, multitap, remapping, mouse, and text input.

## Decisions

### 1. Emulate libpad at the buffer, not the record

The host writes the receive buffer the game registered, not `input_pad1`. Writing records would skip `pad_read_record`'s edge detection, auto-repeat, the analog fold, and demo replay, and would need the game's types on the host side.

`libpad.c` keeps `uint8_t *pad_buffer[2]` from `PadInitDirect` and a `started` flag from `PadStartCom`:

| Call | Host answer |
| --- | --- |
| `PadInitDirect(a, b)` | store `a`, `b`; write `status 0xFF` into both until the first vblank sample |
| `PadStartCom()` | `started = 1` |
| `PadGetState(0x00)` | `PadStateStable` if started, else `PadStateDiscon` |
| `PadGetState(other)` | `PadStateDiscon` |
| `PadInfoMode(p, InfoModeCurExID, 0)` | `0x73` when the current sample is analog, else 0 |
| `PadSetAct`, `PadSetActAlign` | accept; `PadSetActAlign` returns 1 |

Port 2's buffer stays at status `0xFF`; the game sees state Discon first anyway.

### 2. One sample per vblank, after events

`host_vblank` in `libetc.c` becomes: callback, poll events, `host_input_sample()`, present, pace. `host_input_sample` (in `host_input.c`) reads the keyboard and controller into a `HostPad { uint16_t buttons; int analog; uint8_t lx, ly, rx, ry; }` (buttons active high, in `PadButton` bit positions) and then calls `pad_write_buffers(&pad)` in `libpad.c`, which converts to the PS1 bytes:

```
held  = buttons (Start 0x0800, Up 0x1000, …, L2 0x0001, …)
byte2 = ~(held >> 8)    byte3 = ~(held & 0xFF)
byte0 = 0x00            byte1 = analog ? 0x73 : 0x41
byte4..7 = rx, ry, lx, ly  (0x80 when no controller)
```

The game polls once per frame and a frame is two vblanks, so a key held for one vblank can be missed by an edge only if it is released before the game's next poll; a human press lasts several vblanks.

### 3. Mapping as a pure function

`host_input_keys_to_buttons(const Uint8 *keys)` maps `SDL_SCANCODE_*` to buttons, so the startup check can call it with a synthetic key array:

| PC key | PS1 |
| --- | --- |
| Arrows | d-pad |
| `X`, `Z`, `C`, `V` | ✕, ○, □, △ |
| `Enter` (`RETURN`, `KP_ENTER`) | Start |
| `Right Shift` | Select |
| `A`, `S` | L1, R1 |
| `Q`, `W` | L2, R2 |

Scancodes, not key codes, so the layout is positional on non-US keyboards.

`host_input_controller_to_pad(SDL_GameController *)` reads `SDL_CONTROLLER_BUTTON_A/B/X/Y` (south/east/west/north) as ✕/○/□/△, shoulders as L1/R1, `SDL_CONTROLLER_AXIS_TRIGGERLEFT/RIGHT > 16384` as L2/R2, `BACK`/`START`, the d-pad, and both sticks scaled from −32768..32767 to 0..255.

### 4. Controller lifetime

`main_pc.c` adds `SDL_INIT_GAMECONTROLLER`. At startup the host opens the first index for which `SDL_IsGameController` is true. `SDL_CONTROLLERDEVICEADDED` opens one if none is open; `SDL_CONTROLLERDEVICEREMOVED` for the open instance closes it. Those events are handled in the existing `poll_events` loop. Teardown closes it before `SDL_Quit`. No other allocation.

### 5. How the title test presses Enter

The acceptance run posts real key messages to the window (`WM_KEYDOWN`/`WM_KEYUP` with the `Enter` scancode `0x1C` in `lParam`) from `build/host/capture.ps1`, so the test goes through SDL's own keyboard path and `SDL_GetKeyboardState`. It captures the window at the waiting title, presses Enter, waits, presses again, and captures the menu.

## Risks / Trade-offs

- [The window has no keyboard focus] → SDL only updates keyboard state for the focused window. The test brings the window to the foreground first; in normal use the player clicks it.
- [A press shorter than a game frame] → Can be missed by `pressed`; acceptable, as on hardware.
- [Controllers SDL does not know] → Not opened; keyboard still works. SDL's built-in mapping database covers common XInput and PlayStation controllers.
- [Analog type changes what the game does with the d-pad] → The game folds the stick only while the d-pad is idle, as on a DualShock; keyboard-only play stays digital.

## Migration Plan

1. `libpad.c` buffer emulation with checks on states and bytes.
2. `host_input.c` mapping and the per-vblank sample, with checks on the pure mapping.
3. Controller open/close and hot-plug events.
4. The title run with injected Enter presses.
5. Roll back by restoring the disconnected `libpad.c`; nothing under `src/` changes.

## Open Questions

None.
