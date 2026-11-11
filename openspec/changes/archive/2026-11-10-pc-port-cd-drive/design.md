# Design

## Context

See proposal.md for why. How the game uses the drive, from `src/main/cd/afile.c` and `src/main/sound/cd_audio.c`:

- `afile_init` reads `\A_FILE.HDT;1` whole (`cd_read_file`: `CdSearchFile`, `CdControl(CdlSetloc, &file.pos)`, `CdRead(ceil(size / 2048), dest, 0x80)`, then polls `CdReadSync(1, 0)` with `VSync(0)`). The header is 33 little-endian words: the byte position of each `A_FILE.BIN` member.
- It then calls `CdSearchFile(\A_FILE.BIN;1)` and keeps only `afile_base_sector = CdPosToInt(&file.pos)`.
- Every later read (`afile_read_range`, `afile_read_async`, `afile_read_bytes`, `afile_read_sectors`) seeks to `afile_base_sector + (member_pos + offset) >> 11` through `CdIntToPos` and `CdControl(CdlSetloc)`, sets mode `0x80` with `CdControl(0xE)`, calls `CdRead(sectors, buf, 0x80)`, and loops or polls until `CdReadSync` returns `CD_READ_DONE` (0). The byte offset inside the first sector is applied by the game after the read.
- `cd_audio.c` looks up `\STR\01.XA;1` to `\STR\04.XA;1` and streams them with `CdlReadS`. That is music.
- Nothing in `src/` calls `CdReadCallback`, `CdSyncCallback`, or `CdGetSector`.
- `tools/psx_disc.py extract <image> --out <dir>` writes every disc file into `<dir>` with the disc's folders. Form 1 files hold 2048 bytes per sector, trimmed to the file length. XA files (Form 2) are written raw, 2336 bytes per sector. It also writes `license_data.dat`, which is not an ISO file.
- `ps1_main` runs only when `HMBTN_RUN_GAME` is on (`pc-port-vsync-loop`). The PS1 RAM window is mapped at `0x80000000` (`pc-port-ps1-memory`).

## Goals / Non-Goals

**Goals:**

- The game's own sector arithmetic works unchanged: every position it computes from a `CdSearchFile` result lands on the right bytes.
- Disc data comes from files on the PC that a later change can override one by one (the `/mods` goal).
- Missing data fails at startup with a message, not as an endless `CdSearchFile` loop.

**Non-Goals:**

- XA audio, `CdlReadS` sample delivery, and Form 2 data reads.
- Reading the raw image at runtime.
- VRAM uploads; `LoadImage` stays a stub, so the title's textures are read but not shown yet.

## Decisions

### 1. Extract once at build time, with the repository's extractor

`cmake/host.cmake` gets the same `ORIGINAL_DISC` cache path the PS1 build uses. When it is empty and `${CMAKE_SOURCE_DIR}/baserom.bin` exists, it uses that file and says so. A custom command runs `python tools/psx_disc.py extract ${ORIGINAL_DISC} --out ${CMAKE_BINARY_DIR}/disc` once (stamp file), and `hmbtn-host` depends on it. `find_package(Python3 3.10)` is required only on this path.

At runtime the drive opens `HMBTN_DISC_DIR` if that environment variable is set, otherwise `disc/` beside the executable (`SDL_GetBasePath()`).

Alternative considered: reading `baserom.bin` by LBA at runtime and walking its ISO 9660 directory. That gives real disc LBAs with no extraction step, but per-file overrides for mods would need a second layer on top of it, and it duplicates the parser `psx_disc.py` already has. Rejected for now. The drive's interface does not change if a later change adds it as a second backend.

### 2. A virtual LBA layout over the extracted files

`cd_drive_open(dir)` walks the folder recursively, skips `license_data.dat`, and builds a table of extents sorted by upper-case relative path (`/` separators):

```
path         first_lba   sectors                 iso_size          form
A_FILE.BIN   24          ceil(bytes / 2048)      bytes             1
A_FILE.HDT   ...         ceil(bytes / 2048)      bytes             1
STR/01.XA    ...         bytes / 2336            sectors * 2048    2
...
```

The first extent starts at sector 24, after the 16-sector system area and the volume descriptors, so no extent is at sector 0. Each next extent starts right after the previous one. These are not the original disc's LBAs; they do not need to be. The game only adds offsets to the position `CdSearchFile` returned, and never hard-codes an LBA.

`CdSearchFile(fp, name)` normalizes `name`: drop a leading `\`, cut at `;`, turn `\` into `/`, upper-case. It looks the result up in the table. On a hit it sets `fp->pos = *CdIntToPos(first_lba, &pos)`, `fp->size = iso_size`, copies up to 15 characters of the name into `fp->name`, and returns `fp`. On a miss it logs the name once and returns `NULL`, as the SDK does.

Mapping a sector back to bytes is a binary search for the extent with `first_lba <= lba < first_lba + sectors`. A Form 1 extent gives file offset `(lba - first_lba) * 2048`. A Form 2 extent and an address in no extent give zeros.

### 3. CdRead is synchronous; the drive keeps the seek position

| Call | Host behavior |
| --- | --- |
| `CdInit()` | Opens the drive if it is not open; returns 1. |
| `CdControl(CdlSetloc, p, r)`, `CdControlB`, `CdControlF` | Sets `seek_lba = CdPosToInt(p)`; returns 1. |
| `CdControl(other, ...)` | Accepted, ignored; returns 1. |
| `CdRead(n, buf, mode)` | Fills `n * 2048` bytes at `buf` from `seek_lba` onward, `seek_lba += n`, records the result, calls the read callback with `CdlComplete`; returns 1. Returns 0 only when the drive is not open. |
| `CdReadSync(mode, r)` | 0 after a successful read, -1 once after a file I/O error (the game restarts the read). |
| `CdSync(mode, r)` | `CdlComplete`. |
| `CdReadCallback(f)` | Stores `f`, returns the previous one. |
| `CdIntToPos(i, p)` | `i + 150` as BCD minute, second, sector. |
| `CdPosToInt(p)` | The inverse, minus 150. |

`buf` is the address the game passes, used as is. The 24-bit mask from `pc-port-ps1-memory` applies only to ordering-table links; the game never masks a CD destination. Destinations are in the PS1 RAM window (for example `STAGING_BUFFER = region_base + 0x4290`, `overlay_low`) or in host statics (`game_state`), which are all mapped.

`mode` is ignored apart from documenting it: the game passes `0x80` (double speed, 2048-byte sectors). A file handle per extent is opened on first use and kept for the process; reads use `fseek` and `fread`.

### 4. Overlay loads now write real bytes into the low region

`LOAD_OVERLAY` reads each overlay member into `overlay_low` / `overlay_high` (`0x800B7E20` / `0x8012DDE8`). The host already runs that code from its image, so the bytes in the window are not executed. They are useful: overlay data tables that hold absolute PS1 addresses, such as `family_tables` in `family.c` (`0x80127F98`), now point at loaded data instead of zeros.

### 5. Startup refuses a missing archive

With `HMBTN_RUN_GAME` on, `main_pc.c` calls `cd_drive_open` and checks `A_FILE.HDT` and `A_FILE.BIN` before `ps1_main`. If either is missing it logs the disc folder it looked in and the missing name, tears down, and returns 1. With the game off, the drive is not required.

## Risks / Trade-offs

- [Another file than `A_FILE.*` turns out to be read by a hard-coded LBA] → No such read exists in `src/` today; every `CdSetloc` position is derived from `CdSearchFile` or `afile_base_sector`. A future one shows up as zeros at that sector, not a crash.
- [XA lookups succeed but deliver no samples] → `cd_audio.c` state machine keeps restarting `CdlReadS`; it waits with `CdSync(1, ...)` and does not block the frame. Music stays silent.
- [The next scene stalls on another missing host feature] → The acceptance run uses the watchdog from `pc-port-vsync-loop`. A new stall past vblank 41 is recorded with its call chain and handled in a follow-up change, not inside this one.
- [The image is committed by accident] → `.gitignore` gets `/baserom.bin`.
- [Disk space] → The extracted folder is about the size of the image's user data (under 200 MB) under `build/host/`, which is ignored.

## Migration Plan

1. `.gitignore`, the CMake extraction step, and the folder next to the executable.
2. `cd_drive.c` with the extent table, name lookup, and sector reads; `libcd.c` on top of it; startup checks against the extracted files.
3. Startup refusal, then the game run with `HMBTN_RUN_GAME=ON`.
4. Roll back by leaving `ORIGINAL_DISC` unset and `HMBTN_RUN_GAME` off; the window and host loop keep working.

## Open Questions

None.
