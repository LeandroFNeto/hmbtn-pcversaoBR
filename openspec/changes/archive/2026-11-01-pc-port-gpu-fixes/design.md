# Design

## Context

See proposal.md for why. The host already draws ordering-table polygons by sampling the static 1024 × 512 VRAM in `libgpu.c`. Three gaps in that path matter here.

The frame clear is black. `main.c` sets both draw environments to `isbg` with `r0 = g0 = b0 = 0`, and `PutDrawEnv` clears the OpenGL frame. The fragment shader discards a texel whose 15-bit word is 0. A primitive that is not emitted, or whose samples are all 0, is therefore a black rectangle the shape of that hole.

The draw buffers are `(0, 0, 320, 240)` and `(0, 240, 320, 240)` (`main.c`). `StoreImage` copies the static array only (`vram.c`). The game's own stores do not read those buffers: CLUT rows (`clut.c`), item pictures (`item_storage.c`), the photo scratch `(896, 256, 64, 256)` (`photo_album.c`), the save scratch `(448, 0, 64, 256)` (`swind.c`), and the event stash `(320, 0, 128, 255)` (`event_script.c`). A store that does overlap a draw buffer still sees whatever was last uploaded there, not the frame.

`gte_h_SetBackColor`, `gte_h_SetColorMatrix`, and `gte_h_SetLightMatrix` ignore their arguments. `model_load_lighting` calls them. `src/include/gte.h` never issues `ncs`, `nccs`, or `dpcs`, and `model_draw_tmd_object` copies the TMD color words into the packet. `scene_set_lights` calls `GsSetFlatLight`, `GsSetAmbient`, and `GsSetLightMode`, which are empty, and `model_draw_visible` does not read those lights.

`SetTexWindow` writes `0xE2000000` and ignores the `RECT`. `SetDrawMode` does the same for its second word when the rectangle is non-null. The walker ignores `0xE2`. The shader wraps U with `mod(floor(vUV), 256.0)`. `DRAWENV.tw` is left zero by `SetDefDrawEnv`, and the only `SetDrawMode` call (`staff.c`) passes a null window. No farm call sets a non-zero window.

## Goals / Non-Goals

**Goals:**

- Meet the three delta specs without allocating inside `DrawOTag` or `StoreImage`.
- Keep CLUT and scratch reads on the static array.
- Pick the scenery fix from a capture, not from a guess.

**Non-Goals:**

- Drawing lines, dithering, or the mask bit.
- Implementing `ncs` / `dpcs`, or recoloring `POLY_G4` / `POLY_GT4` from the light registers.
- Filling in `GsSetFlatLight`, `GsSetAmbient`, or `GsSetLightMode`.
- Editing `src/main`, `src/overlay`, or `src/include`.

## Decisions

### 1. Refresh the draw buffer only where the rectangle overlaps it

`PutDrawEnv` records the current clip in four static integers (origin and size). `StoreImage`, before the existing copy, intersects the rectangle with that clip. The intersection is read from the back buffer with `glReadPixels` after `flush_verts`, one row at a time, into one static row of 320 RGBA bytes and one static row of 320 `uint16_t`. Each pixel uses the same viewport scale as `gpu_debug_read_pixel`. Screen `(sx, sy)` lands at VRAM `(clip.x + sx, clip.y + sy)`. The 15-bit word is the top five bits of R, G, and B. The row is written with `vram_load`, then the original `vram_store` copies the whole rectangle, overlap included.

A miss (CLUT row, photo scratch, event stash) does not call `glReadPixels`.

Alternative considered: render the frame into a 1024 × 512 framebuffer object so every VRAM pixel is a GPU pixel. Rejected: the game draws in 320 × 240 screen space, and the static array is already the texture source. A second full-VRAM target would duplicate it.

The PS1 semi-transparency bit is not in the OpenGL pixel. Readback stores it clear. The spec requires the 15-bit color, which this keeps.

### 2. Store the three GTE registers, and do not light packets

Same pattern as `gte_set_far_color`: three static components and two static 3 × 3 matrices, copied by the setters, exposed through `gte_debug_settings`. `SetBackColor(10, 20, 30)` reads back `(10, 20, 30)`. `SetFarColor` shifts by 4 because the library's far-color registers do; these three are specified unshifted. The matrix setters copy the nine `short` entries and leave the translation alone.

`model_draw_tmd_object` is not wrapped, and no lighting command is added. A self-check draws a gouraud packet whose corner colors differ from the back color and reads a corner pixel back.

Alternative considered: apply `SetLightMatrix` and `SetColorMatrix` while emitting `POLY_GT4` / `POLY_G4`. Rejected: the packet colors are already the TMD words, and nothing in the command set consumes the registers. Doing it would change the PS1 picture.

### 3. Texture window as batch state

The GPU word `0xE2` (psx-spx) holds, in steps of 8 pixels, mask X in bits 0–4, mask Y in 5–9, offset X in 10–14, and offset Y in 15–19. Sampling is `c = (c AND NOT (mask << 3)) OR ((offset AND mask) << 3)`, per axis. Mask 0 leaves `c` unchanged. `U = 9` with an X mask of 8 and offset 0 becomes `1`.

`SetTexWindow` and the second word of `SetDrawMode` pack a `RECT` the way the SDK macro does: `w` and `h` are the masks in pixels, `x` and `y` the offsets, each divided by 8. A null `RECT` in `SetDrawMode` still writes a zero second word, which is not an `0xE2`. A zero rectangle writes `0xE2000000` (window off).

The walker, on `0xE2`, stores the four pixel values in static integers and flushes the current batch if they changed. The fragment shader applies the formula with those uniforms before the existing page and CLUT fetch. GLSL 1.20 has no bitwise operators, so the five mask bits are applied with arithmetic, unrolled. The vertex buffer does not grow. The window is not cleared at the end of `DrawOTag`.

Alternative considered: fold the window into the vertex UVs on the CPU. Rejected: the mask applies per fragment, and vertices of one quad can sit on different sides of the wrap.

This is implemented even though no current farm list sets a non-zero window. The scenery task does not assume the window is the cause.

### 4. The black rectangles, and which fix is allowed

What is already known:

- The hole is the black clear, or a primitive whose samples are all word 0.
- It is not missing GTE lighting (decision 2).
- It is not a texture window the game already requested (no such call, `DRAWENV.tw` is zero). A window the diagnostic finds inside a real packet is a different fact, and decision 3 then covers it.

What is not known, and is decided by one traced frame of the farm and one of the flashback, logged with the existing trace switch and then removed from the hot path:

- Whether any `0xE2` in that frame has a non-zero mask.
- Whether the center of each black region is covered by an emitted primitive. If it is, log the command, page, CLUT, UVs, and the VRAM word at the sample.
- Whether a line (`0x40`–`0x5F`) or a fill (`0x02`) shares a packet with a later quad. Today a line command returns from the packet immediately, and a fill is one skipped word, so the rest of that packet is misread.

Then exactly one of these:

- A non-zero window is in effect for the covering primitive. Decision 3 is the fix. Confirm with the capture.
- The covering primitive samples word 0 while the tile it should show is elsewhere on the same page. Fix the address in the sampler. Do not invent a window the packet did not set.
- No primitive covers the center, and a line or a fill ate the rest of a packet. Skip that command's own words (a line is 3, 4, 5, or 6 words from its code; a fill is 3) and keep walking. Do not draw the line.
- None of those. Stop. Write the log into the task and do not add a fourth theory in code.

No branch allocates, and none edits the game sources.

## Risks / Trade-offs

- [Readback misses the semi-transparency bit] → The stored word matches the visible color. A later blend that needed the bit from a screen copy is unchanged from today and stays out of scope.
- [The diagnostic's frame is not the black one] → The task names the three views in the spec (beside the house, the bed, the flashback) and checks those pixels, not an early title frame.
- [`glReadPixels` of a full buffer is slow] → Only the overlapping rows are read, and the game's current stores miss the draw buffer, so play does not read the frame back. The self-check reads an 8 × 8 block.
- [GLSL 1.20 arithmetic drifts off the bit formula] → The self-check's window case is the spec's `U = 9 → 1`, compared on the CPU against the same formula before it is trusted in the shader.

## Migration Plan

Host-only. Reverting `libgpu.c`, `libgpu_stub.c`, `vram.c`, and `gte.c` restores the previous picture and the previous `StoreImage`. No save or disc data changes.

## Open Questions

None. The scenery branch is selected by the trace in decision 4, and a branch that matches none of the three stops rather than widening the change.
