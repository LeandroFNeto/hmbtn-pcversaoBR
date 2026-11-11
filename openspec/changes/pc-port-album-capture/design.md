# Design

## Context

See proposal.md for why. Draws land in the OpenGL back buffer. `vram[][]` changes only on `LoadImage`, `MoveImage`, `ClearImage`, and the draw-buffer overlap that `StoreImage` already refreshes. `gpu_refresh_draw_overlap` reads that overlap one row at a time into `refresh_rgba` and `refresh_words`, packs the top five bits of each channel, and writes the source with `vram_load`. `MoveImage` and ordering-table `0x80` copy `vram[][]` only. The album rectangle in `photo_album.c` is `(896, 256, 64, 256)`. `upload_vram` sends dirty rows with one `glTexSubImage2D` into the single VRAM texture before the next batch.

## Goals / Non-Goals

**Goals:**

- A copy that meets the album rule writes the drawn 15-bit pixels into the destination, then the existing dirty-row upload puts those rows in the texture a later sprite or quad samples.
- The back-buffer read for this rule runs inside that copy and nowhere else.
- The read uses `refresh_rgba` and `refresh_words`. The transfer adds no buffer and no `malloc`.

**Non-Goals:**

- Reading the back buffer on present, or on a frame that does not issue the copy.
- A second texture, a resample of 320 by 240 into 64 by 256, or a color that was not in the frame.
- Changing `StoreImage` of `(896, 256, 64, 256)`, `LoadImage` of the saved scratch, the blend equation, or `pc-port-scenery-fixes`.
- Editing `src/main`, `src/overlay`, or `src/include`.
- Treating the disc picture `texture_pack_load` draws in the viewer as this capture.

## Decisions

1. **Refresh the source, then move.** When the destination overlaps `(896, 256, 64, 256)` and the source overlaps the current draw clip, call `gpu_refresh_draw_overlap` on the source rectangle, then the existing `vram_move`. The refresh writes the overlap into the source. The move then copies those words to the destination and marks the destination rows dirty. `flush_verts` already uploads dirty rows before it draws, so a textured primitive later in the chain, or in a later frame, samples the captured words from the same VRAM texture.

   The clip is the one `gpu_put_draw_env` stored. That is the buffer the frame was drawn into. The display environment is not read.

   A destination word whose source sits outside the clip stays the word `vram_move` reads, because the refresh writes only the overlap.

   Alternative considered: `glReadPixels` straight into the destination. Rejected. It would pack the same pixels a second way, and a later `StoreImage` of the source would still see the old mirror. Refreshing the source first keeps one packer.

   Alternative considered: reading the whole back buffer every present into `(896, 256)`. Rejected. The read is blocking, and the album rectangle is not the frame size.

2. **The gate is both rectangles.** `MoveImage` and the `0x80` branch test the gate before the move. `StoreImage` and `LoadImage` do not. A source outside the current clip, or a destination outside `(896, 256, 64, 256)`, takes today's copy. A 16 by 1 row at `x = 896` whose `y` is outside 256 through 511 fails the destination test, so a text CLUT on that column is left alone.

   `0x80` already flushes before `vram_move`. The refresh flushes too. Calling it first, then the move, is enough. An empty flush is the existing no-op.

3. **No new storage.** The transfer calls the function that already owns `refresh_rgba` (2048 pixels) and `refresh_words` (320 words). The overlap is at most one draw-buffer width, which that word row holds. No heap buffer, and no allocation per call or per frame.

4. **The constructed frame is the proof.** The self-check draws a flat rectangle of a known color, moves an 8 by 8 block inside it to `(896, 256)` through `MoveImage` and again through `0x80`, and reads the destination words. It repeats the move with the draw env at `(0, 240)`. It then draws a 16-bit sprite that samples `(896, 256)` and reads that pixel back. A move from outside the draw buffer onto the scratch keeps the source word. A 16 by 1 move at `x = 896` and a `y` outside the scratch keeps the source word. `StoreImage` of a scratch filled with a known word, with no capture, still returns that word. A drawn frame with no album copy leaves a sentinel in the scratch.

   The first gated copy in a session logs `album:` with the source rectangle, the destination, and one destination word. The log is one static flag. A photo taken in the game is checked against that line. If the line never appears, the picture task stays open and no pixel is invented.

## Risks / Trade-offs

- [The game never copies the draw buffer into the scratch] → The self-check still proves the rule. The in-game picture stays open, and the disc picture in the viewer is not reported as a capture.
- [`glReadPixels` stalls the GPU] → It runs only inside a gated copy, one row of the overlap at a time.
- [Refreshing the source also updates the draw-buffer mirror] → That mirror then matches the frame, which a `StoreImage` of the same block already does. The refresh stays behind the album gate, so a texture `MoveImage` elsewhere does not pay it.
- [A 4-bit block whose source and destination both meet the gate becomes 15-bit frame pixels] → That is the capture. A copy that misses either rectangle stays a word copy.

## Migration Plan

Host only. Restoring `libgpu.c` and `vram.c` restores the stale scratch. No save or disc data changes.

## Open Questions

None. The in-game photo is a task that stays open when the capture line is absent, not a choice that changes this approach.
