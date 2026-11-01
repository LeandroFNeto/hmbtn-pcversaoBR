# Tasks

## 1. The reported stall

- [x] 1.1 With `HMBTN_RUN_GAME=ON` and the extracted disc, start a new game and go past the name entry (name, then the remaining intro questions) with the input scripts in `build/host/`, logging `main_scene` changes and every `MemCard*`/`Mcx*` call. Verify and record in this file whether the host reaches `SCENE_FARM`. If it stalls, record the scene, the last card call (if any), and a stack sample (`HMBTN_SAMPLE_STACK=1`). If no card call precedes the stall, note that the stall is outside this change, leave the logging off by default, and continue with group 2.

Notes from the run (2026-10-31, `HMBTN_TRACE=1`, `build/host/keys.ps1`): the name, birthday, farm name, and dog name screens all complete inside the title scene (13). After "This is OK" the scene changes to `SCENE_FARM` (2) at vblank 11,598, and nine vblanks later the watchdog reports the game spinning in `host_poll` (the debug trap of `ANI_FAIL`):

```
host_poll            src/main/system.c:10
anim_bind_file       src/main/graphics/anim.c:41
player_init_toys     src/overlay/11_GF_FARM/people/player.c:6289
farm_bring_up        src/overlay/11_GF_FARM/farm_init.c:96
farm_startup_step    src/overlay/11_GF_FARM/farm_main.c:309
farm_enter_dispatch  src/overlay/11_GF_FARM/farm_main.c:60
```

`player_init_toys` binds animations at `farm_area.slotSixArt` + the offset from `farm_slot_tocs[6]`. That address is misaligned or lacks the `.ANI` magic, so the farm's art data was not loaded where the code expects it. No `MemCard*` or `Mcx*` call happens after boot's `MemCardInit`/`MemCardStart`. The stall is outside this change; it is recorded in the README and needs its own change (farm data loading). `HMBTN_TRACE=1` stays as an opt-in diagnostic.

## 2. The card image

- [x] 2.1 Add `src/pc_port/include/memcard.h` and `src/pc_port/src/memcard.c` with the static image, the state, the path (`HMBTN_MEMCARD` or the executable folder + `mcd001.mcr`), the layout and checksums from design.md section 2, open/create/format/unformat, and the commit through `<path>.tmp` and a replace in its own `PC_PORT_NO_PSYQ_TYPES` file. Add `memcard_check.c` to `HMBTN_SELFTEST=1` on a temporary image, and verify:
  - a missing file is created at 131,072 bytes, starts with `MC`, and has 15 free entries with valid checksums;
  - a 1,000-byte file opens as invalid and is unchanged afterwards;
  - a zeroed file opens as unformatted, and formatting makes it ready;
  - the temporary image is deleted and `mcd001.mcr` is not touched.
- [x] 2.2 Add the directory operations (create with block chaining, delete, list with exact name and `*`) and file data access by byte offset along the chain. Verify these self-checks:
  - creating `BASLUS-01115-FARM0` with 4 blocks fills entries 1–4 as first, middle, middle, last, with size 32,768 and the links 1→2→3→4;
  - creating it again reports "already exists";
  - creating two more 4-block files leaves 3 free blocks, and another 4-block file reports "block full";
  - deleting the second file frees its 4 blocks;
  - listing `*` returns every file with its size;
  - 32,768 bytes written at offset 0 read back equal after closing and reopening the image;
  - a write at offset 64 reports "invalid".

## 3. The game-facing libraries

- [x] 3.1 Rewrite `src/pc_port/src/libmcrd.c` over `memcard.c` per design.md section 3: pending result, `MemCardSync` returning 1 once and then −1, `Exist`/`Accept` returning 1, the per-channel state results, the `Open`/`ReadData`/`WriteData`/`Close` path, and `MemCardCallback`. Call `memcard_close` at host teardown. Verify these self-checks:
  - `Accept(0)` then `Sync(0)` gives command 2 with result 0 on a ready card;
  - command 2 with `McErrNotFormat` on an unformatted card;
  - `McErrCardNotExist` on channel `0x10`;
  - `WriteFile` then two `Sync(1)` calls give (1, command 4, result 0), then −1;
  - a registered callback receives (4, 0);
  - no call ever reports `McErrNewCard`.
- [x] 3.2 Give `libmcx.c` the pending `McxCardType` result per design.md section 4, and add `src/pc_port/src/libcard.c` with the `libapi.h` card functions. Verify these self-checks:
  - `McxCardType(0)` returns 1, and `McxSync` then reports `McxErrInvalid` once;
  - `McxCardType(0x10)` leads to `McxErrNoCard`;
  - `McxSync` with nothing pending still returns 1 with `McxErrNoCard`;
  - the MinGW build has no warnings in the new and changed files, and `nm` on `hmbtn-host.exe` shows `InitCARD` and `StartCARD` defined.

## 4. The game

- [x] 4.1 Run the game with no `mcd001.mcr` beside the executable and reach the memory card screen or a save screen. Verify:
  - the file appears at 131,072 bytes;
  - the screen reports a memory card in slot 1 and no card in slot 2 (not "damaged" or "not formatted");
  - the frame counter in the log keeps advancing during the probe.

  Then point `HMBTN_MEMCARD` into a folder that does not exist, so the file cannot be created. Verify the log names the path and the game reports no card instead of stalling.

  Notes from the run (2026-10-31): boot's `MemCardInit` created `mcd001.mcr` (131,072 bytes). CONTINUE on the title opens the load screen (`SCENE_LOAD`):
  - The probe of slot 1 gives Accept → none and nine `McxCardType` → invalid (a plain card).
  - The probe of slot 2 gives nine Accept → no card.
  - The screen then asks for a slot, and the poll alternates `MemCardExist(0x00)` and `MemCardExist(0x10)` with vblanks advancing.
  - Choosing slot 1 reads the three save headers (file not exist, after the game's own 8 tries each), lists the card, and shows "No loadable diary in MEMORY CARD slot 1."
  - Choosing slot 2 shows "There is no MEMORY CARD in MEMORY CARD slot 2."

  With `HMBTN_MEMCARD=C:\nao_existe_hmbtn\mcd001.mcr`, the log has `memcard: cannot create C:\nao_existe_hmbtn\mcd001.mcr; slot 1 is empty` and the screen shows "There is no MEMORY CARD in either slot one or slot two."

  The card buffers are on the heap: with them static, the image grew to 0x102B000, past the 0x1000000 limit `host_image_check.cmake` enforces. The image now ends at 0xF9D000.
- [x] 4.2 Play to the first diary save, save to slot 1, and close the window. If the input scripts cannot reach the diary, ask Leandro to make the save by hand and continue from the saved file. Verify:
  - the log shows the create and the 0x8000-byte write of `BASLUS-01115-FARM0`;
  - no `mcd001.mcr.tmp` is left behind;
  - the host exits with code 0.

  Restart the host and choose CONTINUE. Verify the load screen lists the save with its name and date, and loading it reaches the farm on the saved day. Record the steps and embed a screenshot of the load screen.

  Attempt 1 (2026-11-01, after `pc-port-gte-library`, the real `build/host/mcd001.mcr`): boot opens the card (`memcard: opened ... 15 free blocks`, `MemCardInit(0x00) -> 3`), and the title menu starts on CONTINUE. NEW GAME, the introduction, scene 13 → 2 at vblank 7376, the mayor's first-morning dialogue, then the childhood flashback ("I'm sorry but I've had to cancel the family trip this summer."). While advancing the flashback text the host faults:

  ```
  host fault: code 0xC0000005 at 0x00437311, access 0x00000000
  host fault stack: 0x00436836 0x00433D08 0x00433C3C 0x0042E6CC 0x00416D2A 0x00416C1C 0x0040D3CB ...
  player_build_result         people/player.c:742   result_value = *(s32 *)ground_hit_primitive();
  player_frame_update         people/player.c:475
  farm_run_frame_subsystems   farm_frame.c:106
  farm_mode_tick              farm_frame.c:63
  farm_enter_dispatch         farm_main.c:60
  ```

  The probe of the tile ahead of the player found no ground, so `ground_hit_primitive()` is null and the code reads through it. On the PS1 that read gives the kernel's word 3 at address 0, which has no high bits, so the code's own fallback sets `player_status_word` to 0x8000 ("no ground"). Six places dereference the hit without a null check: `player.c:742` and `:4380`, `farm_probe.c:920`, `field.c:269`, `greenhouse.c:67`, `mine.c:174` (`farm_probe.c:928` and `:943` do check). This is a PS1 null tolerance outside this change; the diary is not reached yet.

  Attempt 2 (2026-11-01, after `pc-port-ground-probe` covered those reads, `HMBTN_TRACE=1`, the real `build/host/mcd001.mcr` with 15 free blocks): NEW GAME, the introduction, the mayor, the whole flashback (in sepia), and the mayor's rules on the field, with no fault. The game then wakes on Spring 2 (Mon 6:00 AM) inside the house. Walk to the blue book on the table under the clock, face it, and press ✕: the diary menu. "Make diary entry and then sleep" opens the save screen (scene 2 → 7); MEMORY CARD slot 1 lists Diary 1 to 3 as "No diary"; Diary 1:

  ```
  memcard: create BASLUS-01115-FARM0, 4 blocks from block 1: ok
  trace: MemCardCreateFile(0x00) -> 0
  memcard: write BASLUS-01115-FARM0, 0x8000 bytes at 0x0: ok
  trace: MemCardWriteFile(0x00) -> 0
  ```

  The game sleeps and wakes on Spring 3 (Tue 6:00 AM). Closing the window: clean teardown in the log, no `mcd001.mcr.tmp` left, and the host's exit code is 0 (measured on a host started and closed the same way).

  Restart: boot opens the card with 11 free blocks; the title menu starts on CONTINUE; CONTINUE (scene 13 → 8), slot 1, and the load screen lists "Diary 1 — 3rd of Spring 1st Year — AAAFarm AAA" (`build/host/load_list.png`). Loading it reads the file (`MemCardReadFile(0x00) -> 0` twice), and the game resumes in the house on Spring 3, Tue 6:00 AM (scene 8 → 14, `build/host/load_done.png`). Loading does not change `mcd001.mcr`.
- [x] 4.3 Open the `mcd001.mcr` from 4.2 in an emulator's memory card manager (DuckStation or ePSXe), or with a short script that parses the directory as the emulators do. Verify it lists `BASLUS-01115-FARM0` with 4 blocks. Then check that `git diff -- src/main src/overlay src/include` is empty.

  A short Python parser of the raw format (header frame, 15 directory frames, XOR checksums, block chain, save header): 131,072 bytes, `MC` header with a valid checksum; entries 1 to 4 are first, middle, middle, last, all checksums valid, 11 entries free; `BASLUS-01115-FARM0` is 32,768 bytes = 4 blocks, chain 1 → 2 → 3 → 4; its first block starts with `SC`, 3 icon frames, title "ＨＡＲＶＥＳＴ　ＭＯＯＮ　ＦＩＬＥ１　Ｙｒ　１　Ｓｐｒｉｎｇ". The diff under `src/` is empty.

## 5. Documentation

- [x] 5.1 Update `README.md` per the README status rule:
  - move the memory card from "Ainda falta" to "Já resolvido";
  - document `mcd001.mcr`, `HMBTN_MEMCARD`, and that the file can be copied to or from emulators;
  - list the remaining card limits (slot 2, PocketStation, editing the image while the host runs);
  - add the result of task 1.1 to "Ainda falta" if the stall was outside this change.

  Verify the README's run instructions match the build.

  2026-11-01: phase 10 in "Já resolvido" now describes the save, the restart, and CONTINUE; "Salvar e carregar um diário" left "Ainda falta" for "Limites do cartão de memória" (slot 2, PocketStation, editing the image while the host runs, which keeps it in memory and replaces the file). `mcd001.mcr`, `HMBTN_MEMCARD`, and the emulator copy were already documented. Task 1.1's stall was fixed by `pc-port-farm-load` (archived), so nothing is added for it. The build and run commands in the README are the ones used in these runs.

## Workflow follow-up

- Archive `pc-port-memory-card` after `pc-port-audio` and `pc-port-seq-player`, once these tasks are checked.
