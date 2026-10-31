# Tasks

## 1. VSync owns the frame

- [x] 1.1 Add host `libetc.h` / `libetc.c` with `VSync`, `VSyncCallback`, and `ResetCallback` per the mode table in design.md, and move event polling, presentation, and pacing into the per-vblank step. Replace the `main_pc.c` loop with a host loop that calls `VSync(0)` while `HMBTN_RUN_GAME` is off. Verify the MinGW build links, the window shows the green quad, and closing it exits with code 0.
- [x] 1.2 Make `DrawOTag` set a game-drew flag that `gpu_draw_demo` does not set, move `glClear` after the swap, draw the green quad only until the game's first `DrawOTag`, and present nothing on a later vblank without a game `DrawOTag`. Verify a host startup check: before any game draw the green quad is presented; a callback drawing a `POLY_F4` in another color presents that color and not the green quad; after that, a vblank with no callback presents nothing.
- [x] 1.3 Add the re-entry guard. Verify a startup check whose callback calls `VSync(0)`: the inner call returns the counter without a second callback, and the outer `VSync` completes.
- [x] 1.4 Add close via `setjmp` / `longjmp` and the 59.94 Hz deadline pacing. Verify `VSync(3)` called ten times takes 0.45 to 0.6 seconds by the performance counter, and closing the window during those calls exits with code 0.

## 2. GPU and CD waits

- [x] 2.1 Make `DrawSync` return 0 for both modes, and give the CD stubs the return table in design.md without writing destinations. Verify a startup check: `CdSearchFile` returns its file pointer, `CdReadSync(0, 0)` returns 0, and a buffer passed to `CdRead` is unchanged.

## 3. Run the game

- [x] 3.1 After `pc-port-psyq-stubs` and `pc-port-ps1-memory` are applied, turn `HMBTN_RUN_GAME` on so `main_pc.c` calls `ps1_main` after `ps1_memory_map` and the window. Verify the boot's ten `VSync(3)` calls present at least 30 vblanks, the window keeps responding, and closing it exits with code 0. If the process exits or faults before that, record the last vblank count and the faulting function and pause.

  Open, recorded 2026-10-31: with `-DHMBTN_RUN_GAME=ON` the boot passes its ten `VSync(3)` and reaches the title at vblank 41, then stalls. Chain from the watchdog and `addr2line`: `main_run_frame` → `title_step_dispatch` → `title_load_sprite_file` (`title.c:3511`) → `anim_bind_file` → `ANI_FAIL()` → `host_poll()` (`system.c:10`). The title `.ANI` buffer is zero because the CD stubs return no data, so the magic check fails and the game spins in `host_poll`, which never calls `VSync`; the window stops responding and close does not exit. Revisit when the `CdRead` change supplies disc data. `HMBTN_RUN_GAME` stays `OFF` by default until then.

  Closed 2026-10-31 with `pc-port-cd-drive`: with the extracted disc the game passes vblank 41, runs about 500 vblanks in ten seconds with no watchdog stall, and closes with exit code 0. Two fixes in this change's code were needed on the way: `ResetCallback` now only initializes on its first call (it was clearing `vsync_frame_callback` before every A_FILE read, so the game never drew), and a vblank without a game draw no longer presents the green quad (it flickered between game frames).
- [x] 3.2 Verify `git diff -- src/main src/overlay src/include` is empty and the link still contains only SDL2 and `opengl32` as libraries.

## Workflow follow-up

- Archive `pc-port-vsync-loop` after review, once these tasks are checked.
