# Proposal

## Why

The host window opens and closes, but nothing from the game can appear in it. The decomp builds GPU packets (`POLY_F4`, `POLY_FT4`, `POLY_GT4`, `SPRT`) and fills their screen coordinates with GTE commands, then submits them through `DrawOTag`. Those calls are still empty stubs, so the SDL window stays blank.

## What Changes

- Attach an OpenGL context to the existing 640×480 SDL window and present a frame from the loop that already handles close.
- Implement `DrawOTag` in the host `libgpu` stub so it walks an ordering table and draws flat quads, textured quads, gouraud textured quads, and sprites as two triangles each. Any other command code is skipped.
- Add a host GTE that turns the rotation, translation, and projection state into the same screen-space X/Y words the packets already store.
- Keep the PlayStation build unchanged. Do not add game translation units to the host target in this change, so the MinGW link that already succeeds stays green after every step.
- Show one host-built `POLY_F4` through `DrawOTag` each frame. That is the visible check until `ps1_main` can run. Do not call `ps1_main` yet.

Assumption: textured primitives draw as colored triangles in this change. VRAM uploads, CLUTs, and PNG mods stay a later change. `POLY_FT4` is in scope because `model.c` emits it; the request named `POLY_F4`, `POLY_GT4`, and `SPRT`.

## Capabilities

### New Capabilities

- `host-gl-present`: The SDL window presents an OpenGL framebuffer each iteration and still closes from the window button.
- `gpu-packet-replay`: `DrawOTag` turns `POLY_F4`, `POLY_FT4`, `POLY_GT4`, and `SPRT` packets into triangles in that framebuffer and ignores every other GPU command.
- `gte-screen-transform`: The host GTE projects a vertex with the current matrix, translation, offset, and screen distance into the screen-space coordinates a packet stores.

### Modified Capabilities

- None. `openspec/specs/` has no archived capabilities yet.

## Impact

- `src/pc_port/src/main_pc.c`, `src/pc_port/src/libgpu.c`, `src/pc_port/include/libgpu.h`, and new host GTE sources under `src/pc_port/`. `cmake/host.cmake` links the system OpenGL import library on Windows. No new package beside the SDL2 the host build already finds.
- Game sources under `src/main/` and `src/overlay/` stay out of the host link. `src/include/gte.h` stays the MIPS header; a host shadow replaces it only for the host target.
- Out of scope: booting `ps1_main`, CD/mod file redirects, real texture sampling, LINE/TILE/DR_MODE drawing, and the 24-bit ordering-table links inside `LINK_PRIM` (those pointers are not unique on Windows until a later change allocates the primitive heap so they survive the mask).
