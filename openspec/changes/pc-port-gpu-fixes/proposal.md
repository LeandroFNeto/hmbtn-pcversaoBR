# Proposal

## Why

The base game is playable, including native saves, and the remaining gap before any mod work is the picture itself. Solid black rectangles sit left of the farmhouse, where the bed should be, and in the childhood flashback, because those pixels are the black clear showing through. The host also never copies a drawn frame back into VRAM, and three GTE register writes the game does make (`SetBackColor`, `SetColorMatrix`, `SetLightMatrix`) still discard their arguments.

## What Changes

- `StoreImage` of a rectangle that overlaps the draw buffer refreshes those words from the OpenGL back buffer (`glReadPixels`) into the static 1024 × 512 VRAM, then copies them out. A rectangle that misses the draw buffer, including CLUT rows and the photo album's scratch at (896, 256), keeps today's static read.
- The three GTE setters store the back color and the color and light matrices the way `SetFarColor` already stores the far color. Polygon colors stay the colors in the packet. The game's GTE command set (`src/include/gte.h`) is `rtps`, `rtpt`, `nclip`, `avsz3`, `avsz4`, `sqr0`, `op12`, `rtv0`, and `rtir`: it never issues `ncs`, `nccs`, or `dpcs`, and `model_draw_tmd_object` copies TMD colors into `POLY_G4` and `POLY_GT4`. Applying the matrices while building those packets would recolor geometry the PS1 leaves alone.
- The walker honors a texture-window word (`0xE2` / `DR_TWIN`). `SetTexWindow` and `SetDrawMode` encode the `RECT` into that word. Sampling applies the PS1 mask, with no allocation in the draw loop. No farm call site sets a non-zero window today (`DRAWENV.tw` stays zero, and the only `SetDrawMode` call passes a null window), so the window is necessary GPU behavior and is not, by itself, the explanation of the black rectangles.
- The black rectangles are fixed by the branch a frame diagnostic selects (design.md). The clear is black (`drawEnv` `r0=g0=b0=0`, `isbg` set), and a texel of `0x0000` is discarded, so a missing or fully transparent primitive is a black rectangle of that shape. Lighting is ruled out. The diagnostic distinguishes a non-zero texture window, a quad sampling empty VRAM, and a quad the host never emits.
- No edits under `src/main`, `src/overlay`, or `src/include`. The MinGW 32-bit host keeps building.

This change is the GPU polish after the playable base. It does not renumber the existing phase 11 (`pc-port-farm-load`).

## Capabilities

### New Capabilities

- `host-framebuffer`: A `StoreImage` that overlaps the draw buffer returns the pixels just drawn, and writes them into the static VRAM first. Other rectangles are unchanged.

### Modified Capabilities

- `gpu-packet-replay`: A texture window masks UVs while sampling, and the farmhouse side, the bed, and the flashback background are no longer solid black.
- `host-gte-library`: `SetBackColor`, `SetColorMatrix`, and `SetLightMatrix` keep the values the game writes. They do not recolor packets.

The main `gpu-packet-replay` spec still says textured packets are not sampled. The host already samples VRAM (`pc-port-vram`, not synced). This change does not rewrite that requirement.

`host-vram` is not a main spec yet. `pc-port-vram` already defines `StoreImage` as a read of the static array. Refreshing the draw-buffer overlap before that read leaves that requirement true, so this change does not add a second `host-vram` delta.

## Impact

- `src/pc_port/src/libgpu.c` (window state, sampler, readback), `libgpu_stub.c` (`SetTexWindow`, `SetDrawMode`), `vram.c` (`StoreImage`), `gte.c` (three registers on the existing settings snapshot).
- `HMBTN_SELFTEST=1` gains checks for the registers, one texture-window word, and a readback of a drawn rectangle. The picture check is a capture of the farm and the flashback.
- Out of scope: lines, dithering, the mask bit, `GsSetFlatLight` / `GsSetAmbient` / `GsSetLightMode` (empty stubs; `model_draw_visible` never reads them), GTE `ncs`/`dpcs`, mods, and a higher internal resolution.
