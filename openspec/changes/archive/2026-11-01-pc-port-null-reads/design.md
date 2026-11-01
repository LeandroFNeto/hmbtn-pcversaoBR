# Design

## Context

See proposal.md for why. What the investigation found (2026-10-31, `pc-port-farm-load` task 3.1):

- The fault: `0xC0000005` at `scene_resolve_selection+176` (`scene.c:291`), access 0x00000000. Stack: `scene_enter` (`scene.c:331`) ← `farm_frame_update` (`farm_main.c:592`) ← `farm_startup_step` (phase `FARM_STARTUP_FRAME`) ← `farm_enter_dispatch` ← `main_run_frame` ← `ps1_main`.
- `gdb`: `farm_area.mode.data`, `.previous`, and `.exit` are null, `farm_area.mode.place` is `{0, 0}`.
- `farm_area` is one main-executable global (`src/main/game/farm_state.c`), the same object for every overlay on the PS1 and on the host. Only farm code writes `mode.data`: `farm_startup` (`farm_init.c:273`), `scene_enter` (`scene.c:381`), `farm_load.c:79`, and `clock_events.c:158`. A new game reaches the farm through entry step 1 (`farm_startup_step` → `farm_bring_up`), which sets none of them, so `data` is null on the PS1 too when line 291 runs.
- `data->head.place` is a `LimitPair` (`s16 value`, `s16 location`) at offset 0 of `FarmModeScript`. Line 291 reads `value`; lines 293, 302, and 304 read `value` and `location` the same way.
- psx-spx ("Kernel (BIOS)", "Garbage Area at Address 00000000h"): the first 16 bytes of RAM hold `00000003h` (after boot smashes the first word), `275A0C80h`, `03400008h`, and `00000000h`. The word at 0x60 is the RAM size in MB (2), at 0x64 is 0, at 0x68 is 0xFF. So on the PS1 the read gives `value = 3`, `location = 0`: neither `PLACE_MINE_43` (0x43) nor `PLACE_MINE` (0x37), and both mine branches are skipped.
- The host maps PS1 RAM at 0x80000000 (`ps1_memory_map`), which is KSEG0, the same physical memory as address 0 on the PS1. Nothing on the host uses 0x80000000 to 0x8000FFFF: that is the PS1 kernel's area (the executable's base is 0x80010000, `src/main/CMakeLists.txt`), and a 24-bit link of 0 is a terminator, never an address there.
- `hmbtn_host_patch` today makes one replacement per file, and a second call on the same file would start again from the original.
- The watchdog (`log_main_stack` in `ps1_memory.c`) already walks the `EBP` chain of the main thread; the crash filter (`log_crash`) logs only the fault line. The host is built without optimization, so game functions keep frame pointers.

## Goals / Non-Goals

**Goals:**

- The farm's first frame on a new game reads and writes what the PS1 reads and writes.
- The next fault of this kind can be found from the log.

**Non-Goals:**

- Catching every null access at run time.
- Null writes that would reach the kernel words the host seeds (0x0 to 0xF, 0x60 to 0x6B), or past the kernel area (0x10000). If one turns up, it is recorded, not covered here.

## Decisions

### 1. Low PS1 RAM holds the kernel's values

After `ps1_memory_map` succeeds, write the psx-spx values at 0x80000000 (0x0 to 0xF, 0x60, 0x64, 0x68). The rest of the kernel area stays zero, as `VirtualAlloc` gives it.

`src/pc_port/include/ps1_null.h` defines:

```c
#define PS1_NULL_PTR(p) \
    ({ __typeof__(p) ps1_null_p = (p); ps1_null_p != 0 ? ps1_null_p : (__typeof__(p))PS1_RAM_BASE; })
```

It evaluates `p` once and keeps its type, so `PS1_NULL_PTR(farm_area.mode.data)->head.place` compiles as the original. `PS1_RAM_BASE` comes from `ps1_memory.h`, which `ps1_null.h` includes. The same helper serves writes (Decision 5): `PS1_NULL_PTR(p)->flags |= x` writes through whichever pointer it picked.

Alternatives considered:

- **A separate static block with the same 16 bytes.** It works for this read, but writes and other reads of low RAM through 0x80000000 would see different memory, and the block needs a size bound. The real mapping has neither problem.
- **Map page 0 itself.** Windows refuses allocations below 0x10000.
- **A vectored exception handler that emulates the faulting access.** It would cover every null access without patches, but it has to decode x86 loads and stores (`mov`, `movsx`, `movzx`, `cmp`, `and`, `or` with memory operands) and resume. That is fragile, and it hides the accesses instead of listing them.
- **Set `farm_area.mode.data` to `&farm_area.mode.script` from the host before the farm starts.** That changes game state that the PS1 leaves null, and it reads `script` (0) instead of the garbage area (3). The result is the same here only by chance.

### 2. Several replacements per patch

`hmbtn_host_patch(relative from to [from to ...])` applies the pairs in order to one copy. Configure fails on the first `from` not found, naming the file and that text. `hmbtn_host_patch_header` (from `pc-port-farm-load`) gets the same signature through the shared `_hmbtn_write_patched`. The existing calls keep working unchanged.

The scene patch:

```cmake
hmbtn_host_patch(src/overlay/11_GF_FARM/world/scene.c
    "#include \"scene.h\"" "#include \"scene.h\"\n#include <ps1_null.h>"
    "farm_area.mode.data->head.place" "PS1_NULL_PTR(farm_area.mode.data)->head.place")
```

`string(REPLACE)` changes every occurrence. Elsewhere in `scene.c`, the same text runs only after `data` is set, where the helper passes the pointer through, so wrapping all occurrences is safe. Implementation counts and lists them in tasks.md.

### 3. The crash log lists the stack

`log_crash` walks `info->ContextRecord->Ebp` like `log_main_stack`, with the frame checked against the thread's stack bounds (`NtCurrentTeb()->StackBase` and `StackLimit`) and for 4-byte alignment instead of `IsBadReadPtr`. It logs up to 16 return addresses on one line after the fault line:

```
host fault: code 0xC0000005 at 0x0044E39B, access 0x00000000
host fault stack: 0x0044E486 0x0042C718 0x0042C03E ...
```

One line keeps the log greppable and goes straight into `addr2line -e hmbtn-host.exe`.

### 4. The run, and further null reads

The same run as `pc-port-farm-load` 3.1: `build/host/keys.ps1` with `HMBTN_TRACE=1`, no memory card, through "This is OK", then 30 seconds watching the trace, the watchdog, and the frame counter, with a capture.

If another fault shows an access below 0x10000 through a game pointer, covering it in this change needs two checks, recorded in tasks.md:

- the pointer is null on the PS1 too (who sets it, and why it is not set yet);
- the PS1 read through it gives what the host's low RAM gives.

If both hold, the site gets a replacement in the patch of its file. A null write is covered the same way (Decision 5). Any other fault, stall, or wrong picture is recorded and stops the run.

### 5. Null writes into the kernel's unused RAM

The first run after the scene patch found a null write (tasks.md 4.1, run 1): `scene_slot_flag_move` (`scene.c:1265` and `1266`) updates the flags of both marked scene entries, and an area with no marked model leaves both null. The `u16` at offset 0x28 lies in the PS1 kernel's unused area (0x10 to 0x3F), so on the console the write changes nothing anyone reads.

The same helper covers it: the write lands at 0x80000028 on the host, as at 0x28 on the PS1. Two more pairs go into the scene patch:

```cmake
    "scene_marked_entries[0]->" "PS1_NULL_PTR(scene_marked_entries[0])->"
    "scene_marked_entries[1]->" "PS1_NULL_PTR(scene_marked_entries[1])->"
```

This wraps lines 1265, 1266, 1277, 1278, 1282, and 1283. In `scene_spouse_marker_update` (1273 on), entry 0 is checked but entry 1 can still be null when the area marks only one model; the PS1 writes at 0x28 there too.

A covered write needs one check beyond those of Decision 4: its offset is outside the words the host seeds (0x0 to 0xF, 0x60 to 0x6B) and below 0x10000. A write that would hit them is recorded and stops the run, since it would change what later null reads see in a way this change does not model.

Alternative considered: patching the function to skip the write when the pointer is null. Simpler, but it departs from what the PS1 does, and a later null read of the same offset would see a different value.

## Risks / Trade-offs

- [The patch text matches a spot where `data` could be null for another reason] → The helper then reads the kernel's values there too, as the PS1 does. The covered list in tasks.md says where.
- [psx-spx values differ by BIOS version] → The documented variant at offset 4 (`275A0C50h` in older BIOS) is never read by the covered site (offsets 0 to 3 only). Use the newer value and note the variant.
- [A null read past 0x10000 bytes] → It would land in the executable's PS1 data, not the kernel area. The PS1 does the same, and the check in Decision 4 catches a mismatch.
- [A covered null write changes low RAM a later null read sees] → The same happens on the PS1. Decision 5 keeps writes off the seeded words.
- [Each null access needs a patch] → Accepted. Each one is a real PS1 quirk, and listing them keeps them visible to anyone who later fixes the decomp's own data.

## Migration Plan

1. Low RAM values, `ps1_null.h`, and the self-check.
2. Multi-replacement patches and the scene patch.
3. The crash stack.
4. The run into the farm; further sites, reads or writes, as they show up.
5. Roll back by removing the scene patch; the low RAM values and the crash stack are harmless on their own.

## Open Questions

- Does the PS1 BIOS used by the game's retail target leave `00000003h` in the first word? psx-spx says it does "soon" after boot. The covered read compares the halfword against 0x37 and 0x43, so `3C1A0000h` (halfword 0) would give the same result.
