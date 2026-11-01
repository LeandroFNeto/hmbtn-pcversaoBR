# Tasks

## 1. The table fix

- [x] 1.1 Add `hmbtn_host_patch_header` to `cmake/host.cmake` per design.md section 1, call it for `src/main/graphics/anim.h` (`s32 start[0];` → `s32 start[];`), and put the patched directory after `src/pc_port/include` and before the game directories. Verify:
  - `build/host/host_patched/src/main/graphics/anim.h` has `s32 start[];`;
  - compiling `farm_data.c` with the host flags and `-fsyntax-only` gives no "excess elements" warning;
  - configuring with the patch text changed to something absent fails and names `anim.h`.
- [x] 1.2 Add `src/pc_port/src/farm_tables_check.c` per design.md section 3 and hook it into `HMBTN_SELFTEST=1` in `main_pc.c`. Verify the self-test log shows all 67 tables passing and the known slot-six and pack values, and that the new file compiles without warnings under `-Wall -Wextra`.

## 2. The build guard

- [x] 2.1 Add `cmake/host_cc_check.cmake` as the host target's compiler launcher per design.md section 2. Verify:
  - a clean build passes and the image check still reports an end below 0x1000000;
  - temporarily building with the header patch removed fails on `farm_data.c` with the file and line in the error;
  - the README build commands still work as written.

## 3. The farm

- [ ] 3.1 With `HMBTN_RUN_GAME=ON`, run the introduction with `build/host/keys.ps1` and `HMBTN_TRACE=1`, and confirm the summary. Verify:
  - the trace shows scene 13 → 2;
  - no watchdog stall report appears within 30 seconds;
  - the frame counter keeps advancing.

  Embed a capture of the farm. If the host stalls or crashes again, record the stack (`addr2line`, and `gdb -p` for the data involved) here and stop for a new change.

  Notes from the run (2026-10-31, `HMBTN_TRACE=1`, `build/host/keys.ps1`, no memory card): the old stall is gone. `farm_bring_up` and its `player_init_toys` complete, and after "This is OK" the trace shows scene 13 → 2 (vblank 6310; 5576 in the run under `gdb`). Right after, the host faults before the farm draws a frame:

  ```
  host fault: code 0xC0000005 at 0x0044E39B, access 0x00000000
  #0 scene_resolve_selection (selection=0)   src/overlay/11_GF_FARM/world/scene.c:291
  #1 scene_enter (enter=1, cached=0)         src/overlay/11_GF_FARM/world/scene.c:331
  #2 farm_frame_update ()                    src/overlay/11_GF_FARM/farm_main.c:592
  #3 farm_startup_step ()                    src/overlay/11_GF_FARM/farm_main.c:334 (FARM_STARTUP_FRAME)
  #4 farm_enter_dispatch ()                  src/overlay/11_GF_FARM/farm_main.c:59
  #5 main_run_frame ()                       src/main/main.c:400
  #6 ps1_main ()                             src/main/main.c:374
  ```

  Data (`gdb`): `farm_area` is at 0xC35BC0; `farm_area.mode.data`, `.previous`, and `.exit` are all null, `farm_area.mode.place` is `{0, 0}`, and `modeScene` and `currentMode` are -1. Line 291 reads `farm_area.mode.data->head.place.value` with `data` still null on the first farm frame of a new game. On the PS1, address 0 is kernel RAM, so the read does not fault (what it returns there is not yet checked); on the host, page 0 is not mapped. Not fixed here: it needs its own change (null reads the PS1 tolerates). The 30-second watch, the frame counter, and the farm capture wait on it.
- [ ] 3.2 Check that `git diff -- src/main src/overlay src/include` is empty. Then update `README.md` per the README status rule:
  - add the row for `pc-port-farm-load`;
  - move "A fazenda não carrega" out of "Ainda falta", or replace it with the next blocker from 3.1;
  - note that `pc-port-memory-card` tasks 4.2 and 4.3 are reachable.

Tasks 3.1 and 3.2 were moved, still open, to `pc-port-null-reads` tasks 4.1 and 4.2 when this change was archived (2026-10-31): the farm run waits on that change's fix.

## Workflow follow-up

- Resume `pc-port-memory-card` at task 4.2 (save in the diary, close, continue) once 3.1 passes.
- Archive `pc-port-farm-load` after `pc-port-memory-card` is archived or independently, once these tasks are checked.
