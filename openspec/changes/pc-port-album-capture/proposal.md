# Proposal

## Why

The picture the host just drew lives in the OpenGL back buffer. The album scratch at `(896, 256, 64, 256)` is only the static VRAM array, so a copy from the draw buffer into that scratch still plants the words that were already there. The album then samples those words.

## What Changes

- A `MoveImage` or an ordering-table `0x80` whose source overlaps the current draw buffer and whose destination overlaps `(896, 256, 64, 256)` reads that overlap from the back buffer and writes the 15-bit words into the destination before the copy returns.
- The read runs only inside that transfer. A frame that does not ask for the copy does not read the back buffer for the album.
- The destination rows go out through the existing dirty-row `glTexSubImage2D` before a later textured primitive samples them. No second texture is created.
- `StoreImage` of `(896, 256, 64, 256)` still returns the words already stored there. A copy whose source misses the draw buffer is unchanged. The text CLUTs at `x = 896` outside that rectangle are unchanged.
- The transfer uses the row buffers the draw-buffer read already owns. It does not allocate.

## Capabilities

### New Capabilities

### Modified Capabilities

- `host-framebuffer`: A copy from the draw buffer into the album scratch writes the pixels just drawn, and a later textured sample of that scratch shows those pixels. The three existing requirements stay: a `StoreImage` of the draw buffer still returns the frame, a `StoreImage` outside it still returns the stored words, and the read still does not allocate.

## Impact

- Host only: `src/pc_port/src/libgpu.c` (the `0x80` walk and the draw-buffer readback) and `src/pc_port/src/vram.c` (`MoveImage`). The self-check joins the existing `HMBTN_SELFTEST=1` chain.
- `src/main`, `src/overlay`, and `src/include` stay untouched. The blend equation, the scenery change, and the album's disc pictures are not part of this change.
- The only game use of `(896, 256, 64, 256)` found in `photo_album.c` saves that rectangle to RAM and loads it back. It is not itself a copy from the draw buffer. The capture behavior is proven on a constructed transfer. A photo taken in the game stays a later check, and it stays open when that transfer never runs.
