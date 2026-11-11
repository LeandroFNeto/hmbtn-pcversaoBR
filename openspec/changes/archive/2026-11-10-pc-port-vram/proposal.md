# Proposal

## Why

With the disc served by the host (`pc-port-cd-drive`), the game now draws every frame: about 1,300 `DrawOTag` calls and 10,000 packets in ten seconds. The window shows flat gray `(128,128,128)` because textured packets draw only their modulation color: `LoadImage` drops the pixels the game uploads, there is no VRAM to sample, and the shader has no texture. The title's images and palettes are already in PS1 RAM; they need somewhere to land and a way onto the screen.

## What Changes

- A host VRAM: 1024 × 512 16-bit words (1 MB), the PS1 layout, holding the 5-5-5-1 pixels, 4-bit and 8-bit texel indices, and CLUT rows exactly as the game writes them.
- `LoadImage`, `StoreImage`, `MoveImage`, `ClearImage`, and `ClearImage2` read and write that VRAM. `DrawOTag` also runs the transfers and state commands the game puts in its ordering tables: `DR_LOAD` (`0xA0`, CLUT fades in `clut.c`), `DR_MOVE` (`0x80`), and the draw-mode words `0xE1` (texture page, semi-transparency), `0xE2` (texture window), and `0xE3`–`0xE6` (area, offset, mask; recorded only).
- The OpenGL renderer samples that VRAM. The VRAM is one `1024 × 512` texture, updated with `glTexSubImage2D` for the rows changed since the last draw. The fragment shader decodes 4-bit, 8-bit, and 15-bit texels and their CLUTs, treats `0x0000` as transparent, and modulates by the packet color unless the packet is raw.
- `DrawOTag` draws every polygon (`0x20`–`0x3F`: flat, gouraud, textured, 3 and 4 vertices) and every rectangle (`0x60`–`0x7F`: tiles and sprites, 1×1, 8×8, 16×16, free size) with the PS1's four semi-transparency modes. Lines stay skipped.
- `ClearImage` over the displayed area and `PutDrawEnv` with `isbg` also clear the OpenGL frame, so backgrounds the game clears are not left from the previous frame.
- A texture-source seam: every VRAM write goes through one observer, and every textured draw asks one resolver for its texture. Both are pass-through in this change; a later change plugs `/mods` HD replacements in there.
- No change under `src/`.

## Capabilities

### New Capabilities

- `host-vram`: The host keeps the PS1's 1024 × 512 VRAM and serves every CPU and ordering-table transfer into it, out of it, and inside it.

### Modified Capabilities

- `gpu-packet-replay`: Textured packets sample VRAM and CLUTs instead of drawing a flat color; the drawn set grows from four packet types to all polygons and rectangles; draw-mode words and semi-transparency are applied.

## Impact

- `src/pc_port/src/libgpu.c` (renderer and walker), `libgpu_stub.c` (transfers move out of it), new `src/pc_port/src/vram.c` and `src/pc_port/include/vram.h`, `host_check.c`, `main_pc.c` (GL teardown).
- No new package. OpenGL 2.1 with GLSL 1.20, as today.
- Out of scope: drawing into VRAM (the game's own framebuffer region stays an OpenGL frame, so `StoreImage` of the screen returns what was last uploaded there, not the rendered picture), lines, dithering, mask-bit tests, the texture window's effect on sampling, and loading `/mods` files.
