# Tasks

## 1. Low PS1 RAM

- [x] 1.1 After a successful `ps1_memory_map`, write the psx-spx kernel values at 0x80000000 per design.md section 1, and add `src/pc_port/include/ps1_null.h` with the helper (`PS1_NULL_PTR` since 2.3). Extend `ps1_memory_self_check` (`HMBTN_SELFTEST=1`) to check the words at 0x80000000, 0x80000004, 0x80000008, 0x8000000C, and 0x80000060, and that the helper yields 0x80000000 for a null pointer and passes a non-null one through. Verify the self-test log passes with no other check failing.

## 2. The scene patch

- [x] 2.1 Let `hmbtn_host_patch` and `hmbtn_host_patch_header` take several `from to` pairs per design.md section 2. Verify:
  - the existing `text.c` and `anim.h` patches produce the same copies as before;
  - configuring with a second pair whose text is absent fails and names the file and that text.
- [x] 2.2 Patch `src/overlay/11_GF_FARM/world/scene.c` per design.md section 2. Verify:
  - `build/host/host_patched/src/overlay/11_GF_FARM/world/scene.c` includes `ps1_null.h` and wraps every `farm_area.mode.data->head.place`; list here how many occurrences there are and their lines, and confirm that each one outside `scene_resolve_selection` runs with `data` already set;
  - the build passes the `host_cc_check` launcher with no new warnings.

  Covered reads (lines of the original `scene.c`), 6 in total:
  - 291, 293, 302, 304 (`scene_resolve_selection`): the null read on a new game's first farm frame. `farm_bring_up` never sets `data`; `scene_enter` sets it at line 381, after calling this function at line 331.
  - 193 (`scene_daily_announce`): called from `farm_run_frame_subsystems` during play, after startup's `scene_enter`; `data` is set.
  - 432 (`scene_enter`): after line 381; `data` is set.

  The patched copy compiles with the same four warnings as the original (`csqrt` from `libgte.h`; implicit `PushMatrix`, `RotMatrixZXY`, `PopMatrix`).

- [x] 2.3 Rename the helper to `PS1_NULL_PTR` (in `ps1_null.h`, its self-check, and the scene patch) and add the `scene_marked_entries` pairs to the scene patch per design.md section 5. Verify:
  - the patched copy wraps every `scene_marked_entries[0]->` and `scene_marked_entries[1]->`; list their lines here;
  - the self-test still passes, and the patched copy compiles with the same warnings as the original.

  Covered writes (lines of the original `scene.c`), 6 in total, all `u16` read-modify-writes of `flags` at offset 0x28: 1265 and 1266 (`scene_slot_flag_move`, both entries null in an area without marked models), and 1277, 1278, 1282, 1283 (`scene_spouse_marker_update`, after its check of entry 0; entry 1 can still be null). No `PS1_NULL_READ` is left. The self-test logs the low RAM check passing, and the copy has the same four warnings as the original.

## 3. The crash stack

- [x] 3.1 Make `log_crash` log a stack line per design.md section 3. Verify by running the host built before 2.2 (or with the scene patch commented out) through the introduction: the log shows the fault line and a stack line, and `addr2line -e hmbtn-host.exe` on those addresses gives `scene_enter`, `farm_frame_update`, `farm_startup_step`, `farm_enter_dispatch`, `main_run_frame`, and `ps1_main`.

## 4. The farm

- [x] 4.1 With `HMBTN_RUN_GAME=ON` and no memory card, run the introduction with `build/host/keys.ps1` and `HMBTN_TRACE=1`, and confirm the summary. Verify:
  - the trace shows scene 13 → 2;
  - no fault and no watchdog stall report appear within 30 seconds;
  - the frame counter keeps advancing (the host's exit line counts at least 1,800 vblanks after the scene change).

  Embed a capture of the farm. A further null read is covered here only after the two checks in design.md section 4, both recorded here. Anything else: record the log, the stack, and the `gdb` data here, and stop for a new change.

  Run 1 (2026-10-31, after 2.2): the read at `scene.c:291` goes through, scene 13 → 2 at vblank 5268, then a null **write** faults in the same first farm frame:

  ```
  host fault: code 0xC0000005 at 0x0044FFE7, access 0x00000028
  host fault stack: 0x0044ED3E 0x0042C9AC 0x0042C2BE 0x0042BB44 0x00414114 0x00414094 0x0040A846 ...
  #0 scene_slot_flag_move     scene.c:1265 (patched copy 1266)
  #1 scene_populate_objects   scene.c:529
  #2 farm_frame_update        farm_main.c:597
  #3 farm_startup_step        farm_main.c:334
  ```

  Data (`gdb`): `scene_marked_entries` is `{0x0, 0x0}`; `farm_area.mode.area` 0, `place` `{0, 1}`, head `sceneId` 999; the 15 model flags are 0x800, 0x400, 0x200, 0x800, 0x400, 0x200, 0x2000, 0x4100, 0x5101, 0x5102, and 0x8000 five times, none with `SCENE_ENTRY_MARKED` (0x10). The area's data is loaded and has no marked model, so both pointers are null on the PS1 too: lines 1265 and 1266 read-modify-write the `u16` at 0x28, the PS1 kernel's unused area (0x10 to 0x3F), and the console goes on. `scene_spouse_marker_update` (line 1273) checks `scene_marked_entries[0] != 0` before the same writes; `scene_slot_flag_move` does not. The design then listed null writes as a non-goal, so the run stopped here; the change was extended to cover writes into the kernel's unused RAM (design.md section 5, task 2.3).

  Run 2 (2026-10-31, after 2.3): the read at 291 and the writes at 1265 and 1266 go through, scene 13 → 2 at vblank 5330, then another null access in the same first frame, which this change does **not** cover:

  ```
  host fault: code 0xC0000005 at 0x0044927D, access 0x00000000
  host fault stack: 0x0042C9CA 0x0042C2BE 0x0042BB44 0x00414114 0x00414094 0x0040A846 ...
  #0 field_probe_sweep (restart=0)   field/field.c:269   if (hit->header & 8)
  #1 farm_frame_update               farm_main.c:601 (PLACE_FARM branch)
  #2 farm_startup_step               farm_main.c:334
  ```

  Data (`gdb`): `field_row` 0, `field_column` 1; `field_probe_point` `{-3800, 31000, 1500}`; `ground_found` 0, `ground_tolerance` 200, `ground_dy` 31000; `farm_area.mode.quads[0]` is `{0x80187178, 0 × 44}`. The probe at column 0 hit the ground (its quad is stored), and `farm_settle_on_ground` then set the point's height to `ground_height_at()` = 31000, although the triangle it found lies within ±200 of y = 0. At column 1 no triangle is within 200 of 31000, `ground_hit_primitive()` is null, and line 269 reads through it.

  Cause: `ground_height_at` (`src/main/geometry/ground.c:76`) computes the plane normal with `OuterProduct0`, which is an empty stub on the host (`src/pc_port/src/libgte_stub.c:133`); `normal` keeps stack garbage. On the PS1 the sweep fills the field grid, so this is not a PS1 null quirk and fails the first check of design.md section 4. 22 `libgte` functions the game calls are empty stubs on the host: `InitGeom`, `ratan2`, `csqrt`, `SquareRoot0`, `rsin`, `rcos`, `RotMatrix`, `RotMatrixX`, `RotMatrixY`, `RotMatrixZ`, `RotMatrixZXY`, `TransMatrix`, `PushMatrix`, `PopMatrix`, `ApplyMatrixLV`, `RotTrans`, `OuterProduct0`, `VectorNormal`, `VectorNormalS`, `VectorNormalSS`, `SetFarColor`, `SetFogNear`. They need their own change; the run stops here.

  Run 3 (2026-11-01, after `pc-port-gte-library`, no debugger): scene 13 → 2 at vblank 5412; 40 seconds on the farm with no fault and no stall report; the host's exit line counts 7812 vblanks, 2400 after the scene change. The farm draws and the mayor starts the first-morning dialogue (`build/host/clean_farm.png`). No further null access showed up, so the covered sites stay the 6 reads and 6 writes in `scene.c`. Details and the field-sweep check are in `pc-port-gte-library` task 3.1.

- [x] 4.2 Check that `git diff -- src/main src/overlay src/include` is empty. Then:
  - update `README.md` per the README status rule: add the row for `pc-port-null-reads`, mark the `pc-port-farm-load` row as archived, move "A fazenda não carrega" to "Já resolvido" or replace it with the next blocker, and say whether `pc-port-memory-card` tasks 4.2 and 4.3 are reachable.

  4.1 and 4.2 also carry the open tasks 3.1 and 3.2 of `pc-port-farm-load`, archived on 2026-10-31.

  2026-11-01: the diff under `src/` is empty. README: phase 11 (`pc-port-farm-load`, archived), 12 (this change) and 13 (`pc-port-gte-library`) are in "Já resolvido"; "A fazenda não carrega" is gone, replaced by the black rectangle seen on the farm; `pc-port-memory-card` 4.2 and 4.3 are listed as reachable.

## Workflow follow-up

- Resume `pc-port-memory-card` at task 4.2 (save in the diary, close, continue) once 4.1 passes.
- `pc-port-farm-load` is archived and `openspec/specs/host-source-compat/spec.md` exists, so this change can be archived on its own.
