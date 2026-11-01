# Design

## Context

See proposal.md for why. What the investigation found (2026-10-31):

- The watchdog stack at the stall is `host_poll` ← `anim_bind_file` (`src/main/graphics/anim.c:41`, the alignment test) ← `player_init_toys` (`player.c:6288`) ← `farm_bring_up` ← `farm_startup_step` ← `farm_enter_dispatch`.
- With `gdb` attached to the stalled host:
  - `anim_bind_file` received `file = 0xC52FCD`;
  - `&farm_area.slotSixArt` is `0xC52FC8`, and its first words are `0x0E 0x001F000F …`, the same bytes as `A_FILE.BIN` at 0x380B000 (the `.ANI` magic is 0x0E);
  - `*farm_slot_tocs[6]` has `count = 5`, and the word after it is `farm_slot_toc_2`'s count, also 5.
- `ANIM_PACK_START(toc, k)` is `(&(toc)->count)[(k) + 1]`. With the offsets dropped from the initializer, `k = 0` reads past the struct.
- GCC 6.3 (`MinGW.org GCC-6.3.0-1`) reports "excess elements in array initializer" for `farm_data.c` lines 10–161, 105 lines in total, and for no other game source. The CMake build does not show these warnings, so they went unnoticed.
- The tables are `farm_slot_tocs[7]`, `farm_item_tocs[10]`, and `farm_pack_tocs[50]`. Slot and item offsets start at 0. Pack offsets start after the pack header, at 4 × (`count` + 1). `farm_item_toc_2` is not monotonic, and some pack tables list one offset more than `count + 1`.
- `AnimPackToc` is used only through pointers (`farm_load.c`, `player.c`, `chicken.c`, `farm_data.h`), never as an array element or an inner struct member.
- `cmake/host.cmake` already has `hmbtn_host_patch(relative from to)`. It compiles a copy of a `.c` file under `build/host/host_patched/` with one text replacement, and configure fails if the text is not found. Today it patches `text.c` only.

## Goals / Non-Goals

**Goals:**

- The farm's tables of contents hold the same values as on the PS1.
- The fix lives in the host build, and a recurrence anywhere in the game fails the build.

**Non-Goals:**

- Fixing whatever the farm hits next. It is recorded for its own change.
- Changing the PS1 build or `src/`.

## Decisions

### 1. Patch `anim.h`, not `farm_data.c`

Add `hmbtn_host_patch_header(relative from to)` next to `hmbtn_host_patch`:

- It writes the patched copy under `build/host/host_patched/<relative>` with the same "not found" failure.
- It collects the copy's directory in `HMBTN_PATCHED_INCLUDE_DIRS`.
- `target_include_directories(... BEFORE ...)` lists `src/pc_port/include`, then `HMBTN_PATCHED_INCLUDE_DIRS`, then the game directories.

The one call is `hmbtn_host_patch_header(src/main/graphics/anim.h "s32 start[0];" "s32 start[];")`.

A flexible array member as the last member keeps `sizeof(AnimPackToc)` at 4 and `start` at offset 4. GCC accepts static initializers for it as an extension, and the values land right after `count`, exactly where GCC 2.95 put them.

`#include "anim.h"` from a file inside `src/main/graphics/` still finds the original first, because quote includes search the including file's own directory first. Those files (`anim.c` and its neighbours) only read tables through pointers. Both declarations have the same layout, so the mix is harmless; the self-check and the gameplay run confirm it.

Alternatives considered:

- **Patch `farm_data.c` itself.** A text replacement there would have to retype 67 definitions, or slip a second struct type in through a macro. That is more fragile than one token in the header.
- **Switch every table to a different initializer form in a host-only file.** That duplicates game data in `src/pc_port/`, which drifts from the decomp.

### 2. Fail the build on dropped initializers

Set `CMAKE_C_COMPILER_LAUNCHER` for the host target to `cmake -P cmake/host_cc_check.cmake`. The script:

- runs the real compiler command it is given;
- passes its output and exit code through;
- turns the run into an error when the output contains "excess elements in array initializer", repeating the offending file and line.

That way the check runs only on the sources that are actually recompiled, at the cost of one CMake start per file.

Alternative considered: a configure-time scan of all game sources with `-fsyntax-only`. It takes about 20 s on every configure and misses header-only changes between configures. The launcher is exact and incremental.

### 3. Self-check of the tables

A new `src/pc_port/src/farm_tables_check.c` declares the three `extern AnimPackToc *` arrays (with `anim.h` from the patched include path) and checks:

- for every table, the first-offset rule from the spec, and that the offset at `count` is greater than the first;
- the slot-six values `0, 1008, 3072, 3688, 4236, 5376` and the first pack table `12, 1008, 2200`.

It logs one line per failing table, plus one summary line with the number of tables checked, and joins the `HMBTN_SELFTEST=1` chain in `main_pc.c`.

### 4. The gameplay run

Use `build/host/keys.ps1` (from `pc-port-memory-card`) with `HMBTN_TRACE=1` to go from the title through the introduction. After "This is OK", watch for 30 seconds:

- the scene trace;
- the watchdog;
- a capture of the farm.

If another stall or crash appears, record its stack (via `addr2line`, or `gdb -p` for data) in tasks.md and stop there.

## Risks / Trade-offs

- [A file in `src/main/graphics/` initializes an `AnimPackToc` statically] → None does today. The launcher check would catch it, since that file keeps the original header.
- [The launcher slows the build] → One CMake process per compiled file, about 0.1 s each, or roughly 15 s on a clean build of 145 files. Incremental builds barely notice.
- [The farm has more host-only problems after this one] → Expected. The change only promises the farm draws and runs for 30 s. Later blockers are recorded and get their own change.
- [GCC drops the flexible-array initializer under some flag] → GCC documents static initialization of flexible array members as supported. The self-check fails loudly if it ever is not.

## Migration Plan

1. The header patch and include order; rebuild; self-check of the tables.
2. The launcher check.
3. The gameplay run into the farm, then `pc-port-memory-card` tasks 4.2 and onward.
4. Roll back by removing the `hmbtn_host_patch_header` call; nothing else depends on it.

## Open Questions

None.
