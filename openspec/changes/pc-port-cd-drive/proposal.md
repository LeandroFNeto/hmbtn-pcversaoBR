# Proposal

## Why

`ps1_main` now runs on the host and reaches the title at vblank 41, then stalls in `host_poll()`: `anim_bind_file` rejects the title `.ANI` because the CD stubs return no data. The game cannot go further until its CD reads deliver the disc's real bytes. The disc image is at the repository root as `baserom.bin` (MODE2/2352, 213,954,384 bytes).

## What Changes

- The host build extracts the disc's files from the image with the repository's own `tools/psx_disc.py extract`, into `build/host/disc/`, the same folder layout the PS1 build extracts to `original/disc/`. The image path is `-DORIGINAL_DISC=`, defaulting to `baserom.bin` at the repository root when it exists.
- `libcd` becomes a host CD drive over that folder. It assigns each file a sector range (LBA), so `CdSearchFile` returns a real position and size, `CdControl(CdlSetloc)` seeks, `CdRead` copies whole 2048-byte sectors with `fopen`/`fread` into the caller's buffer, `CdReadSync`/`CdSync` report completion, and `CdIntToPos`/`CdPosToInt` convert positions the way the drive does.
- `main_pc.c` opens the drive before `ps1_main` and stops with a logged error if `A_FILE.HDT` or `A_FILE.BIN` is missing, instead of letting the game spin in `CdSearchFile`.
- `baserom.bin` is added to `.gitignore`, so the copyrighted image cannot be committed by accident.
- With the drive in place, task 3.1 of `pc-port-vsync-loop` (run the game, keep the window responsive, close with exit 0) is run again.

Correction to the request: the game opens only two files by name, `\A_FILE.HDT;1` and `\A_FILE.BIN;1` (`afile_init` in `afile.c`). Everything else, the title `.ANI` included, is read by sector from inside `A_FILE.BIN`: `afile_base_sector` (from `CdSearchFile`) plus a member's byte position shifted by 11. The title image comes from member 0 (`objcg\gf.bin`), not from a `TITLE.ANI` file. The four `\STR\0n.XA;1` streams are also looked up by name for music.

## Capabilities

### New Capabilities

- `host-cd-drive`: The host serves the game's CD calls from the files extracted from the disc image, with positions, sizes, seeks, sector reads, and completion as the PS1 drive reports them.

### Modified Capabilities

- None. `host-frame-loop` is still a change delta in `pc-port-vsync-loop`; its CD requirement was narrowed there to "waits do not hold the loop", and data now belongs to `host-cd-drive`.

## Impact

- `src/pc_port/src/libcd.c` (rewritten), a new `src/pc_port/src/cd_drive.c`, `src/pc_port/include/libcd.h`, `main_pc.c`, `host_check.c`, `cmake/host.cmake`, `.gitignore`.
- Build needs Python 3.10+ (already used by the PS1 build) when `ORIGINAL_DISC` is set. Without it, the host still builds and runs with `HMBTN_RUN_GAME=OFF`.
- Out of scope: XA audio playback (`CdlReadS` streams return no samples), Form 2 sector reads, VRAM uploads (`LoadImage` stays a stub), `/mods` overrides, and any edit under `src/`.
