# Tasks

## 1. The patches

- [x] 1.1 Add the five `hmbtn_host_patch` calls of design.md section 1 to `cmake/host.cmake`. Verify each patched copy under `build/host/host_patched/` includes `ps1_null.h` and wraps exactly the listed reads (count them here), that `farm_probe.c`'s two null-tested reads are unchanged, that each copy compiles with the same warnings as its original, and that the self-test still passes.

  7 reads wrapped (original lines): `player.c` 742 and 4380, `farm_probe.c` 920, `field.c` 269, `greenhouse.c` 67 and 113, `mine.c` 174. Each copy includes `ps1_null.h` once; `farm_probe.c` keeps its two `= ground_hit_primitive();` readers as they were. The five copies compile with the same warnings as the originals (none), and the self-test passes.

## 2. The game

- [x] 2.1 Run a new game past the first-morning flashback (`build/host/keys.ps1`, `HMBTN_TRACE=1`). Verify no fault at the covered reads and that the game reaches free play on the farm. Record any further fault with its stack line.

  2026-11-01: NEW GAME through the mayor's first dialogue, the whole childhood flashback (sepia farm, the woods with the girl, "Remember, you promised..."), and the mayor's rules on the field, with no fault. The game then wakes on Spring 2 (Mon 6:00 AM) in the house, in free play: walking, the diary, a save, and the next morning (`pc-port-memory-card` task 4.2, attempt 2). No other null access showed up. The black area seen on the farm also appears in the flashback and where the bed should be in the house.
- [x] 2.2 Check that `git diff -- src/main src/overlay src/include` is empty, and update `README.md` per the README status rule (add the row for `pc-port-ground-probe`).

  2026-11-01: the diff under `src/` is empty; README phase 14 added, and the black-area item now lists the flashback and the house.

## Workflow follow-up

- Resume `pc-port-memory-card` task 4.2 (the diary save) after 2.1.
