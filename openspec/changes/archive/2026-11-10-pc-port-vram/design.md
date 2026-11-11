# Design

## Context

See proposal.md for why. What the game does with VRAM, from the sources:

- Texture packs (`texture_pack.c`) decode run-length data and call `LoadImage` one row or one column at a time, then wait on `DrawSync(1)`. `image_bank_upload` loads whole images. Mode 1 sets bit 15 (STP) on pixels so they blend.
- CLUT fades (`clut.c`) read two 16-color rows back with `StoreImage`, blend them on the CPU, and queue the result as a `DR_LOAD` packet (`SetDrawLoad` plus 13 data words) in ordering table slot 0. The GPU writes it when it reaches that packet.
- Texture animations (`scene_view.c`, `mg5.c`, `picture_cells.c`) copy blocks inside VRAM with `MoveImage`. Photo and save screens (`photo_album.c`, `item_storage.c`, `swind.c`) `StoreImage` and later `LoadImage` regions back.
- `screen_clear_rect` calls `ClearImage` on (0, 0, 320, 480) every frame while `main_clear_screen_enabled` is 1, and on the whole VRAM at boot.
- Sprites carry a CLUT but no texture page; they use the page from the last `0xE1` draw-mode word, which `DR_MODE` and `DR_TPAGE` packets put in the chain. Textured polygons carry their own page in the second UV word.
- The title draws `POLY_FT4` and `SPRT`, with `setSemiTrans` and `setShadeTex`. `screen_fade.c` fades with semi-transparent `POLY_F4` and a `DR_MODE`.
- The host renderer (`libgpu.c`) batches flat colored triangles into one vertex buffer and draws them with a color-only shader in a 320 × 240 space scaled to the window. `DrawOTag` recognizes four codes and walks 24-bit links through `ps1_link_to_ptr`.

## Goals / Non-Goals

**Goals:**

- Texels, CLUTs, transparency, modulation, and semi-transparency as the PS1 GPU defines them, so the title's own pixels and palettes appear.
- One upload path into OpenGL that sends only what changed.
- Fixed memory: no allocation per transfer or frame, everything released at exit.
- A seam where `/mods` replaces textures later without touching the walker or the game.

**Non-Goals:**

- Rendering into VRAM. The framebuffer area is not updated by draws, so reading the screen back with `StoreImage` (photo album) returns stale words.
- Lines, dithering, the mask bit, and the texture window's effect on UVs (`0xE2` is recorded only).
- Higher internal resolution and filtering.

## Decisions

### 1. VRAM is a CPU array, mirrored to one GL texture

`vram.c` owns `static uint16_t vram[512][1024]` (1 MB, zeroed in the image). All transfers write it. A dirty row range (`dirty_y0`, `dirty_y1`) grows on every write.

The GL mirror is one `1024 × 512` `GL_LUMINANCE_ALPHA` / `GL_UNSIGNED_BYTE` texture, `GL_NEAREST`, `GL_CLAMP_TO_EDGE`, created once in `gpu_gl_init`. On little-endian hosts a `uint16_t` row is already the byte pair (low byte → luminance, high byte → alpha), so `glTexSubImage2D(0, y0, 1024, y1 - y0 + 1, …, &vram[y0][0])` uploads changed rows with no conversion buffer. The upload runs right before a batch is drawn, only if the range is not empty, and then clears it.

Alternative considered: decode to an RGBA texture per texture page and CLUT on the CPU. Rejected: every CLUT fade and `MoveImage` would invalidate cached decodes, and the game changes CLUTs every frame during fades. Keeping raw words and decoding in the shader makes a CLUT write a 32-byte upload.

### 2. Transfers

| Call or packet | Effect on `vram` |
| --- | --- |
| `LoadImage(rect, p)` | copy `w × h` words from `p`, row-major, wrap x at 1024 and y at 512 |
| `StoreImage(rect, p)` | copy out the same way |
| `MoveImage(rect, x, y)` | copy row by row through a static 1024-word row buffer, top-down or bottom-up by overlap direction |
| `ClearImage(rect, r, g, b)` | fill with `(b >> 3) << 10 \| (g >> 3) << 5 \| (r >> 3)` |
| `DR_LOAD` (`0xA0`) in a chain | `LoadImage` of `code[1]` (y << 16 \| x), `code[2]` (h << 16 \| w), data from `p` |
| `DR_MOVE` (`0x80`) in a chain | `MoveImage` from `code[1]` to `code[2]` with size `code[3]` |

`SetDrawLoad` and `SetDrawMove` in `libgpu_stub.c` are completed to write those words, as the SDK does. Transfers are synchronous; `DrawSync` keeps returning 0.

`ClearImage` over the displayed area (the rectangle intersects `(0, 0, 320, 480)`) also sets a pending GL clear color that the next frame starts from. `PutDrawEnv` with `isbg = 1` does the same with `r0, g0, b0`.

### 3. The walker reads every command word

The walker keeps `GpuState { tpage, abr, depth, page_x, page_y, tex_window }` for the duration of one `DrawOTag`, starting from the last `PutDrawEnv` tpage. For each packet of non-zero length it reads the command byte of the first word after the tag:

- `0x20`–`0x3F` polygon: bit 2 textured, bit 3 four vertices, bit 4 gouraud, bit 1 semi-transparent, bit 0 raw. Vertex stride follows from those bits. A textured polygon's page word updates `GpuState`.
- `0x60`–`0x7F` rectangle: bits 3–4 size (free, 1, 8, 16), bit 2 textured, bit 1 semi-transparent, bit 0 raw. Textured rectangles use `GpuState`'s page and their own CLUT.
- `0x80` move, `0xA0` load: section 2.
- `0xE1`–`0xE6`: every word of the packet whose top byte is in that range updates `GpuState` (so `DR_MODE`'s two words are both read).
- Anything else, lines included: skipped by length, as today.

### 4. One vertex format, one shader, batches by blend state

Each vertex is `x, y` (screen), `r, g, b` (packet color), `u, v` (texel units inside the page), `page_x, page_y, depth` (0 = untextured, 4, 8, 15), `clut_x, clut_y`, `flags` (raw, semi-transparent). All are floats in one interleaved buffer, as today.

The fragment shader, GLSL 1.20:

```
word(x, y)   = texture2D(vram, (x + 0.5, y + 0.5) / (1024, 512)) → lo + hi * 256 (from L and A, each × 255)
4-bit texel  = word(page_x + u / 4, page_y + v) >> (4 * (u % 4)) & 0xF → word(clut_x + index, clut_y)
8-bit texel  = word(page_x + u / 2, page_y + v) >> (8 * (u % 2)) & 0xFF → word(clut_x + index, clut_y)
15-bit texel = word(page_x + u, page_y + v)
color 0x0000 → discard
rgb = ((c & 31), (c >> 5) & 31, (c >> 10) & 31) / 31
unless raw: rgb *= vertex_rgb * 255 / 128, clamped to 1
stp = c >> 15
```

Bit extraction uses `floor` and `mod` on floats, which is exact for values below 2^24. `u`, `v` are interpolated linearly and floored, which matches the PS1's affine mapping for the screen-aligned quads and sprites the title uses.

Semi-transparency, per the PS1 mode `abr`, uses one blend function `glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA)` with the shader choosing alpha:

| abr | blended texel (`stp` set, or untextured) | opaque texel |
| --- | --- | --- |
| 0: B/2 + F/2 | rgb × 0.5, a = 0.5 | rgb, a = 1 |
| 1: B + F | rgb, a = 0 | rgb, a = 1 |
| 3: B + F/4 | rgb × 0.25, a = 0 | rgb, a = 1 |
| 2: B − F | equation `GL_FUNC_REVERSE_SUBTRACT`, rgb, a = 0 | drawn as 2, accepted approximation |

Mode 2 needs a different blend equation, so batches are cut when the walker moves into or out of a mode-2 semi-transparent primitive. All other primitives share one batch, so draw order is kept with one `glDrawArrays` per run. Before a batch draws, the VRAM mirror is uploaded (decision 1).

### 5. The texture-source seam for `/mods`

Two functions in `vram.c`, declared in `vram.h`, sit between the game's data and OpenGL:

- `vram_observe_write(rect, source, kind)` is called by every transfer in section 2 before it writes. The default does nothing. A mods change uses it to hash each uploaded image (for example a sprite sheet the game loads with `LoadImage` at a known page) and record which VRAM rectangle now holds which original asset.
- `texture_resolve(page, clut, uv_rect)` is called for each textured primitive when it is batched, and returns `TEXTURE_FROM_VRAM` in this change. A mods change returns a replacement GL texture and a UV transform instead, so an HD outfit is sampled in place of the 4-bit original. The walker then starts a new batch with that texture bound.

The game, the walker, and the VRAM transfer code do not change when a mods backend is added; it only implements those two functions. Replacement textures are owned and freed by that backend.

### 6. Memory and teardown

- `vram`, the scratch row for `MoveImage`, and the vertex array are static.
- `gpu_gl_init` creates the VRAM texture, the program, and the vertex buffer once. A new `gpu_gl_shutdown` deletes all three, and `main_pc.c` calls it before deleting the context, on normal exit and on the `longjmp` close path.
- No `malloc` in the draw or transfer paths.

## Risks / Trade-offs

- [Draws do not reach VRAM] → Effects that copy the rendered screen (photo album capture, some swind effects) show stale words. Recorded as a non-goal; it is a later render-to-texture change.
- [Mode 2 with mixed STP texels] → Opaque texels in a subtractive textured primitive subtract instead of overwriting. Untextured fades, the common case, are exact.
- [Float precision in bit extraction] → All values are below 65536 and use `floor`/`mod` on exact integers; checked by the 4-bit and 8-bit startup checks.
- [Affine UV on large rotated polygons] → Matches the PS1, which is also affine.
- [Upload cost] → Worst case one full 1 MB upload per batch after a whole-VRAM clear; typical frames upload a few CLUT rows.

## Migration Plan

1. `vram.c` and the transfers, with startup checks on round trips, `MoveImage` overlap, and clear values.
2. The VRAM texture, the new vertex format and shader, and 4-bit / 8-bit / 15-bit sampling checks rendered off-screen and read back.
3. The full walker (polygons, rectangles, draw-mode words, `DR_LOAD`, `DR_MOVE`) and semi-transparency.
4. The seam functions, `gpu_gl_shutdown`, and the title run.
5. Roll back by restoring the color-only shader; transfers into the VRAM array are harmless without it.

## Open Questions

None.
