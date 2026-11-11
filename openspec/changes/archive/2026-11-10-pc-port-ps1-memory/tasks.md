# Tasks

## 1. Map the PS1 address space

- [x] 1.1 Add `-Wl,--large-address-aware`, `-Wl,--defsym,_overlay_low=0x800B7E20`, and `-Wl,--defsym,_overlay_high=0x8012DDE8` to `cmake/host.cmake`, keeping `-Wl,--image-base,0x400000`. Verify the MinGW build with `-DSDL2_DIR=C:/SDL2/SDL2-2.32.10/i686-w64-mingw32/lib/cmake/SDL2` links and the PE header of `hmbtn-host.exe` has the large-address-aware characteristic.
- [x] 1.2 Add `src/pc_port/include/ps1_memory.h` and `src/pc_port/src/ps1_memory.c` with `ps1_memory_map()` mapping `0x80000000` (2MB) and `0x1F800000` (one page), and call it from `main_pc.c` after `SDL_Init`. Verify a startup check writes and reads back `0x801BE1B0` and `0x1F8003FC`, reads zero at `0x801F7000`, and the window still opens with the green quad.
- [x] 1.3 Make a failed mapping log the range and return non-zero before the window loop. Verify by temporarily mapping `0x80000000` twice in a throwaway build: the second attempt logs the range and the process exits non-zero, then remove the throwaway change.

## 2. Decode 24-bit links

- [x] 2.1 Add `ps1_link_to_ptr` and `ps1_ptr_to_link` with the rule in design.md, and switch `DrawOTag` to it, ending at `0x000000`, `0x00FFFFFF`, and links outside both windows. Verify a startup check walks a `POLY_F4` stored at `0x801C0000` and linked as `0x001C0000`, stops at a `0x00FFFFFF` terminator, and stops at a `0x00300000` link without reading it; the green quad still draws through its image-address link.
- [x] 2.2 Add `ClearOTagR` to `libgpu.h` and `libgpu.c`. Verify a startup check clears an 8-entry table in PS1 RAM, walks it with `DrawOTag` from the last entry to the terminator, and draws nothing.

## 3. Image bounds

- [x] 3.1 Add a post-build step that reads `SizeOfImage` from `hmbtn-host.exe` and fails when `0x400000 + SizeOfImage` is at least `0x01000000`. Verify the current build passes, and that the reported end address is printed in the build log.
- [x] 3.2 Verify from a startup check in `ps1_memory.c` that the addresses of `extern u8 overlay_low[]` and `overlay_high[]` are `0x800B7E20` and `0x8012DDE8` (the same symbols `overlay_regions.c` uses), and that `git diff -- src/main src/overlay src/include` is empty.

## Workflow follow-up

- Archive `pc-port-ps1-memory` after review, once these tasks are checked.
