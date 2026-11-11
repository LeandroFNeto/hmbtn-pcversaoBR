# Design

## Context

See proposal.md for why. How the game paces and draws today:

- Boot in `main` calls `ResetCallback`, `CdInit`, `SetMem`, then `VSync(3)` ten times before any graphics setup. After setup it installs `VSyncCallback(vsync_frame_callback)`.
- Each frame of `main_run_frame` runs the scene, then `VSync(1)`, `DrawSync(0)`, `VSync(1)`, sets `vsync_pending_callback = main_draw_ots`, and calls `VSync(2)`. The draw happens in the callback: `vsync_frame_callback` runs `main_draw_ots`, which calls `PutDispEnv`, `PutDrawEnv`, and `DrawOTag` on each ordering table, then polls A_FILE and draws the loading screen.
- `VSync(-1)` is read as a clock in `sound.c` and `screen_fade.c`. `VSync(0)` is used inside CD waits in `afile.c` and in `pdasamp.c`.
- Busy loops with no `VSync` inside: `while (DrawSync(1) != 0)` (main, texture_pack), `while (CdSearchFile(...) == 0)` (afile), `while (CdControl(0xE, ...) == 0)` (afile), `while (CdControlB(...) != 1)` (cd_audio). They end only if the stub returns the finished value.
- `main_pc.c` creates the window and context, runs the host loop, and draws the green quad. `DrawOTag` flushes immediately to the bound framebuffer.

## Goals / Non-Goals

**Goals:**

- `ps1_main` on the main thread, with every SDL and OpenGL call happening inside a host stub the game already calls.
- Pacing at the NTSC rate the game was written for (59.94 Hz, `SLUS_011.15`).
- A close that never returns into game code.

**Non-Goals:**

- Pad translation, sound, CD data, `/mods`, and frame interpolation.
- `DrawSyncCallback`. Nothing in `src/` calls it.

## Decisions

### 1. VSync is the only place the host runs

Host `VSync(mode)` in `src/pc_port/src/libetc.c`:

| `mode` | Behavior |
| --- | --- |
| negative | return `vblank_count`; no wait, no events, no present |
| `1` | return `0` (no hblank counter); no wait |
| `0` | one vblank |
| `n >= 2` | `n` vblanks |

One vblank is `host_vblank()`:

1. Increment `vblank_count`.
2. If a callback is registered and not already running, set `in_callback`, call it, clear `in_callback`.
3. Poll SDL events. On `SDL_QUIT`, call `host_quit()` (decision 3).
4. If the game has never drawn, draw the green quad.
5. If a game `DrawOTag` ran during this vblank or the green quad was drawn, `SDL_GL_SwapWindow` and clear for the next frame. Otherwise present nothing, so the previous frame stays: the game draws from the callback on the first vblank of each `VSync(2)` only.
6. Sleep until `next_deadline`, then add 1/59.94 s. If more than four vblanks late, reset the deadline to now instead of bursting to catch up.

A `VSync` that arrives while `in_callback` is set returns `vblank_count` without doing anything else, because the callback can reach `loading_screen_draw` and CD polling.

`VSyncCallback(f)` stores `f`. `ResetCallback()` clears it on its first call only and does nothing afterwards, as PSY-Q does: `afile.c` calls it before every read, long after `main` installed `vsync_frame_callback`. Both are in host `libetc.c`.

Alternative considered: a game thread that blocks in `VSync` while the main thread runs SDL. Rejected by the requirement, and it would make every host stub a cross-thread handoff.

### 2. Draw tracking lives in libgpu

`DrawOTag` sets `gpu_game_drew = 1` when called from game code. The green quad goes through a separate `gpu_draw_demo()` that does not set it. `host_vblank` reads and clears the flag, and remembers once the game has drawn. `glClear` moves from the host loop to just after the swap, so the next vblank's `DrawOTag` calls land on a clean frame.

`DrawSync(mode)` returns `0` for both modes; drawing is synchronous on the host.

### 3. Close leaves with longjmp

`main_pc.c` does setup, then `if (setjmp(host_exit) == 0) ps1_main();`. `host_quit()` calls `longjmp(host_exit, 1)`. Control lands back in `main_pc.c`, which deletes the context, destroys the window, calls `SDL_Quit`, and returns 0. No game code runs after the jump. The game holds no host resources, so nothing is leaked by skipping its stack.

If `ps1_main` ever returns, `main_pc.c` takes the same teardown path.

Alternative considered: `exit(0)` from inside `VSync`. Rejected because the context and window would never be destroyed in order, and it hides whether teardown ran.

### 4. CD stubs report completion and leave buffers alone

| Call | Returns |
| --- | --- |
| `CdSearchFile(fp, name)` | `fp` (found) |
| `CdControl`, `CdControlB`, `CdRead` | `1` |
| `CdReadSync(mode, result)` | `0` (done) |
| `CdSync(mode, result)` | `2` (`CdlComplete`) |

In this change no destination is written, so reads leave the zeroed PS1 RAM window as it is. `pc-port-cd-drive` replaces the data side with real reads from the extracted disc and keeps this return contract.

### 5. Order of the main entry

1. `gte_startup_check`
2. `SDL_Init`
3. `ps1_memory_map` (`pc-port-ps1-memory`)
4. window, context, `gpu_gl_init`
5. `setjmp`, then `ps1_main`
6. teardown, `return 0`

Without `pc-port-psyq-stubs` and `pc-port-ps1-memory` applied, step 5 is skipped behind `HMBTN_RUN_GAME` (off) and a host loop calls `VSync(0)` repeatedly. That keeps the window, pacing, and close testable before the game links.

## Risks / Trade-offs

- [The game crashes after boot on empty CD data] → The change owes boot VSyncs and close. A crash after boot is recorded with the last `vblank_count` and fixed in the stub that returned bad data.
- [A busy loop polls a stub this change does not cover] → The window stops responding. The fix is that stub's finished value, listed in the CD and GPU table above.
- [Sleep granularity on Windows] → `SDL_Delay` can overshoot by a millisecond or more. Pacing uses the performance counter and sleeps to the deadline, so error does not accumulate.
- [longjmp skips game cleanup] → The game has nothing to clean on the host; host teardown runs in `main_pc.c`.

## Migration Plan

1. Host `VSync`, `VSyncCallback`, `ResetCallback`, `DrawSync`, and draw tracking, driven by a host loop of `VSync(0)` behind `HMBTN_RUN_GAME` off.
2. CD stubs with the return table, checked by a host startup check.
3. After `pc-port-psyq-stubs` and `pc-port-ps1-memory` are applied, turn `HMBTN_RUN_GAME` on and call `ps1_main`.
4. Roll back by turning `HMBTN_RUN_GAME` off; the host loop keeps the window working.

## Open Questions

None.
