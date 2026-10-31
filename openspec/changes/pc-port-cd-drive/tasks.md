# Tasks

## 1. Disc folder from the image

- [x] 1.1 Add `/baserom.bin` to `.gitignore`. Verify `git status --porcelain -- baserom.bin` prints nothing.
- [x] 1.2 In `cmake/host.cmake`, add the `ORIGINAL_DISC` cache path (defaulting to `baserom.bin` at the repository root when it exists), require Python 3.10 only when it is set, and add a stamped custom command running `tools/psx_disc.py extract` into `${CMAKE_BINARY_DIR}/disc` that `hmbtn-host` depends on. Verify a host build extracts `build/host/disc/A_FILE.BIN`, `A_FILE.HDT`, and `STR/01.XA`, and a second build does not extract again.
- [x] 1.3 Verify a host configure with `-DORIGINAL_DISC=` pointing to a path that does not exist fails with that path in the message, and a configure in a tree without `baserom.bin` and without `ORIGINAL_DISC` still builds `hmbtn-host`.

## 2. The drive

- [x] 2.1 Add `src/pc_port/src/cd_drive.c` and `src/pc_port/include/cd_drive.h` with `cd_drive_open` (folder walk, extent table per design.md, `HMBTN_DISC_DIR` or `disc/` beside the executable), name lookup, and sector reads with zero fill. Verify a startup check under `HMBTN_SELFTEST=1` that the table holds `A_FILE.BIN`, `A_FILE.HDT`, and `STR/01.XA`, and that no extent starts below sector 24.
- [x] 2.2 Rewrite `libcd.c` on the drive: `CdInit`, `CdSearchFile`, `CdControl`/`CdControlB`/`CdControlF` seek, `CdRead`, `CdReadSync`, `CdSync`, `CdReadCallback`, and BCD `CdIntToPos`/`CdPosToInt`. Verify startup checks: `\A_FILE.BIN;1` returns its structure with the file's size; `\str\01.xa` is found; an unknown name returns null; sector 0 converts to `00:02:00` and a round trip of sector 12345 returns 12345.
- [x] 2.3 Verify reads against the extracted files in a startup check: one sector at `A_FILE.HDT`'s position equals the file's first 2048 bytes (zero-padded), sector `k = 3` of `A_FILE.BIN` equals bytes 6144 to 8191 of the file, a sector past `A_FILE.HDT`'s end reads as zeros, the seek advances by the sectors read, `CdReadSync(1, 0)` returns 0, and a registered read callback runs once with `CdlComplete`. Replace the old "CdRead leaves the buffer unchanged" check in `host_check.c`.

## 3. Run the game

- [x] 3.1 Make `main_pc.c` open the drive and require `A_FILE.HDT` and `A_FILE.BIN` before `ps1_main` when `HMBTN_RUN_GAME` is on. Verify that with `HMBTN_DISC_DIR` set to an empty folder the process logs the missing file and exits non-zero without a game frame.
- [x] 3.2 Build with `-DHMBTN_RUN_GAME=ON` and the extracted disc, run for ten seconds, then close. Verify `hmbtn-host.log` shows no watchdog stall at vblank 41, the vblank counter in the exit line is above 41 and keeps increasing, and the close exits with code 0. If a new stall appears, record its vblank and call chain here and pause.

  Result 2026-10-31: no watchdog stall; 587 vblanks in about ten seconds; close exit 0. The exit line now also counts game draws: 1310 `DrawOTag` calls and 10180 drawn packets. The frame is flat gray (`128,128,128`), because textured packets draw their neutral color while `LoadImage` is a stub. Getting there needed the `ResetCallback` and present fixes recorded in `pc-port-vsync-loop` task 3.1.
- [x] 3.3 When 3.2 passes, re-run task 3.1 of `pc-port-vsync-loop` with the same build and check it there. Verify `git diff -- src/main src/overlay src/include` is empty.

## Workflow follow-up

- Archive `pc-port-cd-drive` after review, once these tasks are checked.
- Archive `pc-port-vsync-loop` after its task 3.1 is checked.
