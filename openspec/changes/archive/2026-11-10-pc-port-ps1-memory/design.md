# Design

## Context

See proposal.md for why. The facts the approach rests on:

- The host is i686 (MinGW.org GCC 6.3.0), linked at image base `0x00400000`. `ld` 2.28 accepts `--large-address-aware` and `--image-base`. It does not know `--dynamicbase`, so the image is not relocated by ASLR.
- On this 64-bit Windows, a large-address-aware 32-bit test executable reserved and wrote `0x80000000`–`0x801FFFFF` and `0x1F800000`–`0x1F800FFF` with `VirtualAlloc(MEM_RESERVE | MEM_COMMIT)`. Without the flag, `0x80000000` failed.
- Game code that masks pointers: `LINK_PRIM` in `prim.h` (`& 0xFFFFFF`), `ot_insert_prim` in `frame_buffers.c` (`OT_ADDRESS_MASK`), and `ClearOTagR`, which the SDK implements with `0x00FFFFFF` as the terminator. The game never turns a masked link back into a pointer; only the GPU (here, `DrawOTag`) follows it.
- Pointers the game masks come from two places: PS1 RAM (`WORK_REGION` holds the frame records' OTs and the primitive buffer) and host statics (`frame_ots`, `frame_records`, the demo quad).
- `main.c` does arithmetic between `region_base` and `overlay_region_table[OVERLAY_HIGH]` (`main_region_free_bytes`), so both must live in the same address space with PS1 spacing.

## Goals / Non-Goals

**Goals:**

- PS1 RAM and scratchpad at their PS1 addresses, mapped once in startup.
- A single decode rule for 24-bit links, used by every host function that follows or builds a chain.
- No edit under `src/`.

**Non-Goals:**

- Running `ps1_main` or VSync (`pc-port-vsync-loop`).
- Copying overlay `.data` into the window so absolute data addresses such as `family_tables` resolve.
- Expansion RAM (`0x1F000000`) or hardware registers (`0x1F801000`). Nothing in `src/` names them today.

## Decisions

### 1. Map the PS1 address space instead of rewriting addresses

`ps1_memory_map()` in `src/pc_port/src/ps1_memory.c` calls `VirtualAlloc` at `0x80000000` for `0x200000` bytes and at `0x1F800000` for `0x1000` bytes (the 1KB scratchpad, rounded up to a page). Both are committed read/write and start zeroed. If a call returns anything other than the requested base, it logs the range, releases what it got, and returns failure. `main_pc.c` calls it after `SDL_Init` and before `gpu_gl_init`, so a failure never reaches a frame.

Alternatives considered:

- Rewrite `WORK_REGION` and the other literals to a host buffer. Rejected: those literals are in `main.c` and `cpu_asm.h`, and `src/` stays untouched.
- Load the executable image itself at `0x80010000`. Rejected: a preferred base in the upper 2GB depends on loader behavior for each Windows build, and statics at PS1 addresses would also break the 24-bit identity rule below.
- A 64-bit host with a remap table. Rejected: game code stores pointers in `u32` fields, which a 64-bit pointer does not fit.

### 2. Overlay regions are absolute link symbols

`cmake/host.cmake` adds `-Wl,--defsym,_overlay_low=0x800B7E20` and `-Wl,--defsym,_overlay_high=0x8012DDE8` (i686 PE symbols carry a leading underscore). `overlay_regions.c` keeps declaring them as `extern u8 overlay_low[]`. On the PS1 the linker script defines these; on the host the linker does. `LOAD_OVERLAY` writes into the window, which is harmless because overlay code is already linked into the image.

Alternative considered: static arrays in a host source file. Rejected because `main_region_free_bytes` subtracts `overlay_high` from `region_base` and expects the PS1 distance.

### 3. One decode rule for 24-bit links

```
link == 0x000000 or link == 0xFFFFFF      end of chain
link <  0x200000                          0x80000000 + link   (PS1 RAM window)
0x400000 <= link < 0x1000000              link                (host image)
anything else                             end of chain, nothing read
```

The two windows cannot overlap: PS1 RAM masks to `0x000000`–`0x1FFFFF`, and the image starts at `0x400000`. `ps1_link_to_ptr(u32)` and `ps1_ptr_to_link(void *)` live in `ps1_memory.c` and are declared in `src/pc_port/include/ps1_memory.h`. `DrawOTag` and `ClearOTagR` call them. The game's own `LINK_PRIM` and `ot_insert_prim` already produce the same 24-bit values, because `ptr & 0xFFFFFF` is what `ps1_ptr_to_link` returns for both windows.

`ClearOTagR` lives in host `libgpu.c`. It writes the tags the SDK does: entry `i` links to entry `i - 1`, and entry 0 is the terminator.

### 4. Large-address-aware and image bounds are link options

`cmake/host.cmake` adds `-Wl,--large-address-aware` beside the existing `-Wl,--image-base,0x400000`. A post-build check reads the image size from the PE header and fails the build if `0x400000 + SizeOfImage` reaches `0x01000000`. Static packets keep working when the game sources join the link (`pc-port-psyq-stubs`), because the whole image is still inside the identity range.

## Risks / Trade-offs

- [Something else is loaded at `0x80000000` or `0x1F800000`] → Startup logs the range and exits non-zero. Mapping runs before any DLL beyond SDL2 and `opengl32` is loaded; if it ever fails on a machine, map before `SDL_Init`.
- [The image grows past `0x01000000`] → The post-build check fails the build before an unreachable packet can be produced.
- [Overlay data tables hold absolute PS1 addresses] → Reads through them see the zeroed window. `family_tables` is the one found today. This change lists it and leaves it.
- [32-bit Windows without the 4GT option] → The large-address-aware mapping fails there. The supported host is 64-bit Windows running the 32-bit build.
- [Linux] → `mmap(MAP_FIXED_NOREPLACE)` at the same addresses works for 32-bit processes on a 64-bit kernel. Not implemented in this change.

## Migration Plan

1. Add the link options and `ps1_memory.c`. Call the mapping from `main_pc.c`. Rebuild; the window still shows the green quad.
2. Switch `DrawOTag` to the decode rule and add `ClearOTagR`. Keep the green quad path working through a static (image) link.
3. Add the startup checks below and the image-size check.
4. Roll back by removing the link options and the call. Nothing under `src/` changed.

## Open Questions

None.
