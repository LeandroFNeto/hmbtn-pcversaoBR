# Design

## Context

See proposal.md for why. What the game does with the card, from `src/main/main.c`, `src/overlay/15_saveload/saveload.c`, and `src/overlay/31_gf_mcard/mcard.c`:

- Boot calls `MemCardInit(0)`, `MemCardStart()`, and `McxStartCom()` once. Nothing in the game calls `InitCARD`, `StartCARD`, `_bu_init`, `_card_*`, `OpenEvent`, `TestEvent`, `SwEvent`, or `DeliverEvent`; on the PS1 those sit inside libmcrd. The host replaces libmcrd itself, so the game sees the same API.
- Channels: `0x00` is slot 1 and `0x10` is slot 2.
- Save files are `BASLUS-01115-FARM0`, `FARM1`, `FARM2` (4 blocks, 0x8000 bytes each) and `BISLPSP02489-FARMP` (the PocketStation file, 10 blocks).
- `save_card_probe` (retries up to 8 times):
  1. `MemCardAccept(chan)` must return 1, then `MemCardSync(0, &cmd, &result)` gives the state.
  2. On `McErrNone` it calls `McxCardType(slot << 4)`, which must return 1, then `McxSync`.
  3. `McxErrSuccess` means a PocketStation; `McxErrInvalid` after 8 tries means a plain card. `mcard.c` maps them to `MCARD_PDA` and `MCARD_CARD`.
  4. `McErrNewCard` sends the save screen back to probe again, so a card that always answers "new" would loop forever.
- `save_card_poll` uses `MemCardSync(1, …)` with `SAVE_SYNC_BUSY = 0`, `SAVE_SYNC_DONE = 1`, `SAVE_SYNC_IDLE = −1`. When idle it issues `MemCardExist(chan)` and expects the next poll to report command 1.
- `save_card_write_file` and `save_card_read_file` are called once per frame:
  - The first call sees idle (or another command's result), issues `MemCardWriteFile`/`MemCardReadFile` for 0x8000 bytes at offset 0, and returns busy.
  - The next call expects `MemCardSync(1)` to return done with command 4 (or 3) and the result.
- Synchronous calls whose return value is the result: `MemCardGetDirentry` (exact name, then `*` with max 15), `MemCardCreateFile`, and `MemCardFormat`. `save_card_check_space` sums `size / 0x2000` (rounded up) over 15 entries to count used blocks.
- `save_card_read_header` reads 0x80 bytes at offset 0x200 and checks a XOR over the frame.
- Buffers are game pointers. With PS1 RAM mapped at its PS1 address (`pc-port-ps1-memory`) and stack buffers on the host stack, a pointer can be used directly.

## Goals / Non-Goals

**Goals:**

- The game's card code runs unchanged and never waits on the card.
- A save on disk is never left half-written by a crash or a closed window.
- The image is the standard raw format, so it can be exchanged with emulators.

**Non-Goals:**

- Timing of real card access (about 1 ms per frame on the PS1).
- Card swap detection; the "new card" result is never produced.
- Slot 2, multitap channels, and PocketStation emulation.

## Decisions

### 1. The image module (`memcard.c`)

The module keeps one static 131,072-byte copy of the image, the path, and a state: `absent`, `invalid`, `unformatted`, or `ready`.

- `memcard_open(path)` reads the file:
  - missing → create it formatted (section 2) and write it;
  - wrong size or unreadable → `invalid`, and the file is left untouched;
  - no `MC` in frame 0 → `unformatted`;
  - otherwise → `ready`.
- Every change goes through `memcard_commit()`, which writes the whole image to `<path>.tmp`, flushes it, and replaces the card file (`MoveFileExA` with `MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH` on Windows, `rename` elsewhere). The call returns only after that. Writing 128 KB per save is negligible, and the replace means a crash leaves either the old card or the new one, never a mix.
- The Windows call lives in its own file, built with `PC_PORT_NO_PSYQ_TYPES` like the other `windows.h` users.

Alternatives considered:

- Writing only the changed frames in place: less I/O, but a torn write can corrupt the directory.
- Keeping the file open: no gain, and it holds a lock on Windows.

### 2. Card layout

| Block 0 frame | Content |
| --- | --- |
| 0 | `4D 43` ("MC"), zeros, byte 0x7F = XOR of bytes 0–0x7E (0x0E) |
| 1–15 | directory entry for data block 1–15 |
| 16–35 | broken-sector list: bytes 0–3 `FF FF FF FF`, bytes 8–9 `FF FF`, checksum |
| 36–62 | zeros |
| 63 | copy of frame 0 |

Directory entry (frame `n` describes data block `n`):

| Offset | Size | Field |
| --- | --- | --- |
| 0x00 | 4 | state: `0xA0` free, `0x51` first block, `0x52` middle, `0x53` last |
| 0x04 | 4 | file size in bytes (first block only; blocks × 8,192) |
| 0x08 | 2 | next block index, 0–14, or `0xFFFF` for the last one |
| 0x0A | 21 | name, NUL-terminated (up to 20 characters) |
| 0x7F | 1 | XOR of bytes 0x00–0x7E |

A free entry is state `0xA0`, size 0, next `0xFFFF`, empty name. Data block `n` starts at byte `n × 8192`. A file's byte offset `o` lives in its `(o / 8192)`-th block along the chain.

- `MemCardFormat` rebuilds block 0 from this table.
- `MemCardUnformat` zeroes frame 0 and frame 63.

### 3. libmcrd on the host

One pending result (`valid`, `cmd`, `result`) and the registered callback.

| Call | Host action | Returns |
| --- | --- | --- |
| `MemCardInit(val)` | `memcard_open(memcard_path())` once; later calls do nothing | — |
| `MemCardStart` / `Stop` / `End` | nothing | — |
| `MemCardExist(chan)` | pending = (1, state result) | 1 |
| `MemCardAccept(chan)` | pending = (2, state result) | 1 |
| `MemCardReadFile` / `WriteFile` | do the copy (and commit for writes); pending = (3 or 4, result) | 0 |
| `MemCardOpen(chan, name, flag)` | remember the file for the `Data` calls | result |
| `MemCardReadData` / `WriteData` | as `ReadFile` / `WriteFile` on the opened file; pending = (5 or 6, result) | 0 |
| `MemCardClose` | forget the opened file | — |
| `MemCardGetDirentry`, `CreateFile`, `DeleteFile`, `Format`, `Unformat` | done in the call | result |
| `MemCardSync(mode, &cmd, &rslt)` | if pending: copy it out, clear it, return 1; else return −1 | 1 / −1 |
| `MemCardCallback(f)` | store `f`; return the previous one | previous |

State result for slot 1:

- `ready` → `McErrNone`;
- `unformatted` → `McErrNotFormat` (only from `Accept`; `Exist` gives `McErrNone`, since the card is connected);
- `invalid` → `McErrCardInvalid`;
- `absent` → `McErrCardNotExist`.

Any other channel gets `McErrCardNotExist`. File calls on a card that is not `ready` return that same state result.

The callback, when set, is called with the pair right after the command, before the call returns. The game never registers one; this only keeps the API whole.

Return values:

- 1 for `Exist`/`Accept` is what `save_card_probe` checks.
- The `ReadFile`/`WriteFile` return value is not read by the game. 0 ("registered") is the documented success.

Checks on a read or write: offset and length must be multiples of 128, and `offset + length` must fit the file's size. Otherwise the result is `McErrCardInvalid`.

Alternative considered: emulating the kernel card events (`SwEvent`, `DeliverEvent` on `SwCARD` / `HwCARD` descriptors) under libmcrd. The game never opens those events. The real libmcrd turns them into exactly the `MemCardSync` results above, so reproducing the results directly is smaller and observable in the same way.

### 4. libmcx

`McxCardType(port)` records a pending `McxFuncCardType` and returns 1:

- `port >> 4 == 0` with the card not `absent` → result `McxErrInvalid` (a plain card does not answer PocketStation commands);
- otherwise → `McxErrNoCard`.

`McxSync` reports a pending result once, with return 1. Without one, it keeps today's answer: 1, command 0, `McxErrNoCard`. That way `10_PdaSamp` behaves as before.

### 5. libcard

`libcard.c` defines the `libapi.h` card declarations so nothing links against an undefined symbol:

- `InitCARD`, `StartCARD`, `StopCARD`, and `_bu_init` do nothing (`StartCARD` returns 1);
- `_card_info`, `_card_load`, and `_card_clear` return 1.

They do not touch the card image.

### 6. Path and diagnostics

`memcard_path()` returns `HMBTN_MEMCARD` when set, else `SDL_GetBasePath()` + `mcd001.mcr`. This matches how `cd_drive.c` finds `disc/`.

Each open, create, delete, format, and write logs one line with the path or the file name and the result. Reads and probes are not logged, because the probe runs every frame on the save screen.

### 7. Self-check

With `HMBTN_SELFTEST=1`, `memcard_check.c` points the module at a temporary image (`memcard_selftest.mcr` in the executable folder), runs the spec scenarios, deletes the file, and closes the module. `MemCardInit` then opens the real card. The check never touches `mcd001.mcr`.

## Risks / Trade-offs

- [The stall after the name entry is not caused by the card] → Task 1.1 reproduces it first. The card change still stands on its own; the stall gets its own change with the findings.
- [libmcrd return codes that the game does not read differ from the SDK] → Only values the game reads are specified; the others follow the SDK documentation.
- [Replace-on-write fails (antivirus or file lock)] → The commit returns an error, the result becomes `McErrCardInvalid`, the game shows its "failed" message, and the previous card stays intact. The log names the Windows error.
- [Image edited by an emulator while the host runs] → Not detected; the host's copy wins at its next write. Documented in the README.
- [A save from a different game version] → Not this layer's concern: the game checks its own checksum and reports a bad file.

## Migration Plan

1. `memcard.c`: image, layout, open/create/commit, and the directory operations, with the self-check.
2. `libmcrd.c` over it, then `libmcx.c`'s card type and `libcard.c`.
3. The game run: probe, save, close, continue.
4. Roll back by restoring the "no card" stubs; nothing else depends on them.

## Open Questions

None.
