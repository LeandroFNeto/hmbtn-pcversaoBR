# Proposal

## Why

Every memory card call on the host is a stub that answers "no card" (`libmcrd.c`, `libmcx.c`), so the diary cannot save and CONTINUE on the title cannot load. To play past one session, the host needs a memory card that the game's own save/load screens (`15_saveload`) and memory card screen (`31_gf_mcard`) can use unchanged.

The reported stall after the name entry screen has not been reproduced yet: the last run stopped while typing the name. From the code, the new-game path goes from the title to `SCENE_FARM`, and the card is only touched in `SCENE_SAVE`, `SCENE_LOAD`, `SCENE_LIVESTOCK_TRADE`, and `SCENE_MEMCARD`. The first task reproduces the stall and records whether the card is involved; if it is not, this change still delivers saves and the stall gets its own change.

## What Changes

- A virtual memory card in slot 1 backed by a 131,072-byte file (`mcd001.mcr` next to the executable by default, `HMBTN_MEMCARD` overrides the path) in the raw PS1 card layout: 16 blocks of 8 KB, block 0 holding the `MC` header and the 15 directory frames, blocks 1–15 holding file data in 128-byte frames. The same file opens in PS1 emulators.
- A missing file is created already formatted; a file of the wrong size, or with a bad header, is reported to the game the way real hardware reports a damaged or unformatted card.
- `libmcrd` on the host: `MemCardInit`/`Start`/`Stop`/`End`, `MemCardExist`, `MemCardAccept`, `MemCardOpen`/`Close`, `MemCardReadFile`/`WriteFile`, `MemCardReadData`/`WriteData`, `MemCardCreateFile`, `MemCardDeleteFile`, `MemCardFormat`, `MemCardUnformat`, `MemCardGetDirentry`, `MemCardCallback`, and `MemCardSync`. Asynchronous commands finish inside the call (file I/O on a PC is fast enough); their result is reported once by `MemCardSync` with the PS1 command and result codes, exactly where the game polls for it.
- `libmcx` answers the card-type query as a plain memory card (not a PocketStation), which the probe in `saveload.c` and `mcard.c` requires before it accepts a formatted card. Every other PocketStation command keeps answering "no card".
- The kernel card calls in `libapi.h` (`InitCARD`, `StartCARD`, `StopCARD`, `_bu_init`, `_card_info`, `_card_load`, `_card_clear`) get host definitions in a new `libcard.c`. The game never calls them nor the event functions (`OpenEvent`, `TestEvent`, `SwEvent`, `DeliverEvent`) for the card; completion goes through `MemCardSync` and the optional `MemCardCallback`, so no kernel event emulation is added.
- Slot 2 (channel `0x10`) reports no card.
- No change under `src/` outside `src/pc_port/`.

## Capabilities

### New Capabilities

- `host-memory-card`: The host provides a persistent PS1 memory card in slot 1, stored as a raw 128 KB card image, and answers the game's libmcrd and libmcx card calls with PS1 results.

### Modified Capabilities

- None.

## Impact

- `src/pc_port/src/libmcrd.c` (rewritten), `src/pc_port/src/libmcx.c` (card type and sync state), new `src/pc_port/src/memcard.c` with `src/pc_port/include/memcard.h` (the card image), new `src/pc_port/src/libcard.c`, a new `memcard_check.c` self-check, and `main_pc.c` (self-check hook and closing the card on exit).
- Writes a file next to the executable (inside the ignored `build/` folder in the default layout). No new package.
- README: the memory card moves from "Ainda falta" to "Já resolvido", with the card file location and the override.
- Out of scope: slot 2, multitap, PocketStation, memory card hot-swap while running, the `.gme`/`.vmp`/`.psv` formats, and the stall after the name entry if it turns out not to involve the card.
