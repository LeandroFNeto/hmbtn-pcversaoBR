# Proposal

## Why

A new game stops nine vblanks after it enters the farm (`SCENE_FARM`). `player_init_toys` hands `anim_bind_file` the address `slotSixArt + 5`, which is not word-aligned, and the game spins in its debug trap (`host_poll`). The bytes in `slotSixArt` are correct: they start with the `.ANI` magic read from `A_FILE.BIN` at 0x380B000.

The offset is wrong, not the data. `AnimPackToc` ends in a zero-length array (`s32 start[0]`, `src/main/graphics/anim.h`), and `farm_data.c` initializes its tables statically, for example `{5, {0, 1008, 3072, …}}`. GCC 2.95, the PS1 compiler, stored those offsets after `count`. Since GCC 3.0, the compiler warns "excess elements in array initializer" and drops them. On the host, `ANIM_PACK_START(toc, 0)` therefore reads the next variable in memory; for `farm_slot_toc_6` that is the `count` of `farm_slot_toc_2`, which is 5.

A scan of all 113 game sources with the host flags finds this warning only in `farm_data.c` (105 lines). Every slot, season, and item art table of the farm is affected, so nothing in the farm can draw until it is fixed. Saving in the diary (`pc-port-memory-card` tasks 4.2 and 4.3) waits on this change.

## What Changes

- The host build compiles a patched copy of `anim.h` in which `s32 start[0]` becomes the flexible array member `s32 start[]`. GCC keeps the static initializers of a flexible array member, the struct layout and size are unchanged, and `AnimPackToc` is only ever used through pointers. This extends the existing `hmbtn_host_patch` mechanism (today used for `text.c`) to headers. The file under `src/` is not edited.
- The build fails if a game source still produces "excess elements in array initializer", so the same problem cannot come back silently in another file.
- A self-check (`HMBTN_SELFTEST=1`) verifies that the farm's tables of contents hold their offsets: the first offset of every pack is 0, offsets increase, and a few known values match.
- A new game is run past the introduction into the farm. Any further blocker found there is recorded and handed back, not fixed silently inside this change.
- No change under `src/` outside `src/pc_port/`.

## Capabilities

### New Capabilities

- `host-source-compat`: The host build gives constructs that the PS1 compiler accepted the same meaning they had on the PS1, without editing the game sources, and fails loudly where it cannot.

### Modified Capabilities

- None.

## Impact

- `cmake/host.cmake`: header patching and the include order for the patched header, plus the build-time warning check.
- A new `src/pc_port/src/farm_tables_check.c` self-check and its hook in `main_pc.c`.
- `pc-port-memory-card`: tasks 4.2, 4.3, and 5.1 become reachable once the farm loads.
- README: "A fazenda não carrega" moves to "Já resolvido" if the farm reaches play; a later blocker, if any, takes its place in "Ainda falta".
- Out of scope: other farm problems found after this fix (each gets recorded and its own change), and the PS1 build, which keeps compiling the original `anim.h`.
