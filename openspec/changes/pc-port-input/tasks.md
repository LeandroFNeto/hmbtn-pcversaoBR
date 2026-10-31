# Tasks

## 1. The pad port

- [x] 1.1 Rewrite `src/pc_port/src/libpad.c` per design.md section 1: keep the two buffers from `PadInitDirect`, write status `0xFF` into both, track `PadStartCom`, answer `PadGetState`, `PadInfoMode`, `PadSetAct`, and `PadSetActAlign`. Verify `HMBTN_SELFTEST=1` checks: before `PadStartCom` port `0x00` is `PadStateDiscon`; after it port `0x00` is `PadStateStable` and port `0x10` is `PadStateDiscon`; `PadSetActAlign(0x00, …)` returns 1.
- [x] 1.2 Add `pad_write_buffers(const HostPad *)` writing bytes 0–7 of port 1 per design.md section 2, and nothing before `PadInitDirect`. Verify checks: no buttons gives bytes 2 and 3 `0xFF` and type `0x41`; Start held gives byte 2 `0xF7` and byte 3 `0xFF`; ✕ held gives byte 3 `0xBF`; an analog sample gives type `0x73` and its stick bytes; port 2's byte 0 stays `0xFF`; a call before `PadInitDirect` writes nothing.

## 2. Keyboard mapping and the vblank sample

- [x] 2.1 Add `src/pc_port/include/host_input.h` and `src/pc_port/src/host_input.c` with `host_input_keys_to_buttons` per the table in design.md section 3. Verify checks with a synthetic key array: `Enter` alone gives exactly `PAD_START` (`0x0800`); `Up` + `X` gives `0x1000 | 0x0040`; `Right Shift`, `A`, `S`, `Q`, `W`, `C`, `V`, `Z` each give their single button.
- [x] 2.2 Call `host_input_sample()` in `host_vblank` after `poll_events` and before present, combining keyboard and controller and writing the buffers. Verify the MinGW build links, the window still shows the title with `HMBTN_RUN_GAME=ON`, and closing exits with code 0.

## 3. Game controller

- [x] 3.1 Add `SDL_INIT_GAMECONTROLLER`, open the first game controller at startup, handle `SDL_CONTROLLERDEVICEADDED` and `SDL_CONTROLLERDEVICEREMOVED` in `poll_events`, close it in teardown, and map it per design.md section 3. Verify a check of `host_input_controller_buttons` (the pure part that takes button and axis values) that south gives ✕, triggers at 20000 give L2 and R2, and sticks at −32768 and 32767 give 0 and 255. If a controller is attached to this machine, also verify pressing its south button on the title advances it; otherwise record that no controller was available.

  Recorded 2026-10-31: no game controller was attached to this machine (`SDL_NumJoysticks` found none; no `input: controller` log line). The pure mapping checks passed; the hot-plug path was not exercised with hardware.

## 4. The title

- [x] 4.1 Extend `build/host/capture.ps1` (outside the repository's tracked files) to bring the window forward and post `WM_KEYDOWN`/`WM_KEYUP` for `Enter` (scancode `0x1C`). With the extracted disc and `HMBTN_RUN_GAME=ON`, capture the waiting title, press Enter twice a second apart, and capture again. Verify the second capture shows the title menu, not the waiting title, and closing exits with code 0. Embed both captures in the change summary. If the title does not advance, record the vblank and `PadGetState`/buffer bytes seen and pause.

  Result 2026-10-31: the first Enter shows "PRESS START BUTTON", the second the NEW GAME / CONTINUE menu (cursor on CONTINUE). Continuing past the criterion: Enter on CONTINUE opens the memory card slot screen; Down then Enter on NEW GAME starts Mayor Thomas's introduction; the `X` key (✕) advances its text and reaches the name entry screen. Enter does not advance dialogue text; the game advances it with ✕.
- [x] 4.2 Verify `git diff -- src/main src/overlay src/include` is empty and the host still builds with no warnings-as-errors and links only SDL2 and `opengl32`.

## Workflow follow-up

- Archive `pc-port-input` after review, once these tasks are checked.
