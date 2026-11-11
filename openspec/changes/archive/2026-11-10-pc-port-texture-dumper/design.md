# Design

## Context

See proposal.md for why. `host_present` already reads F8 once per presented frame and passes that bit to `objects_dump_present`, `object_trace_present`, and `tmd_vram_present`. Those dumps keep the notes from the frame and write them only on a rising edge. `sample_parts` in `tmd_vram.c` reads a 4-bit texel at `page_x + u / 4`, an 8-bit texel at `page_x + u / 2`, and a 15-bit texel at `page_x + u`, then the CLUT for the indexed depths. The mods loader matches `pageX_pageY_u0_v0_u1_v1.bmp` in one directory and does not walk subfolders.

## Goals / Non-Goals

**Goals:**

- On one rising F8, write one BMP per distinct page and UV bounds drawn in that frame.
- Put the stored color in each pixel, and leave a stored zero black.
- Leave the frame, the VRAM words, and the mods match alone.

**Non-Goals:**

- A dump on every frame while the variable stays set.
- Painting over the shipping bin, or moving the page and UV the game already emitted.
- The bed, the side of the house, and PNG.

## Decisions

1. **The edge is the one `host_present` already has.** `HMBTN_DUMP_TEXTURES=1` is read when the frame is presented. A rising F8 writes the notes collected during that frame and clears them. F8 still down on the next present clears the new notes and writes nothing. The variable unset writes nothing and collects nothing. The same rising test the object dump uses is the one this check drives, by calling the present function with 0 and 1. No second hotkey.

2. **The name and the pixels are the primitive the loader already keys.** The note stores page, depth, CLUT, and the UV bounds `resolve_texture` already computes (`u0,v0` min, `u1,v1` max). The file is `pageX_pageY_u0_v0_u1_v1.bmp`. Width is `u1 - u0 + 1` and height is `v1 - v0 + 1`. Pixel `(i, j)` samples UV `(u0 + i, v0 + j)` with the address in decision context. A 15-bit channel `c` is written as `(c * 255) / 31`. Channel 0 stays 0, so `0x0000` is black. Two notes with the same page and bounds keep one slot.

3. **The files land beside the executable, under `mods/textures/dump/`.** `SDL_SaveBMP` writes them after the frame, from a heap list freed before the next present. The loader's directory is not this subfolder, so a dump is not a replacement until someone moves the file up into `mods/textures` or into `HMBTN_MODS`. The list is not allocated when the variable is unset.

4. **The self-check does not need a play session.** It stores one non-zero CLUT color and one `0x0000`, writes both through the same sampler, and reads the BMP pixels back. It then drives the rising edge: one write, a held F8 writes nothing, and an unarmed F8 writes nothing. A second check leaves the VRAM word unchanged and shows `texture_resolve` still returns 0 for a file that exists only under `dump/`.

## Risks / Trade-offs

- [A farm frame has many textured primitives] → The write happens once per rising F8, after the frame, and duplicate bounds share one file. The next frame collects nothing unless F8 rises again.
- [The shipping-bin sample is stored `0x0000`] → The BMP pixel is black. That is the mold. This change does not paint a bin into it.
- [A 256 by 256 UV span is a large file] → UV bounds are bytes, so that is the maximum, and it is one file per rising edge rather than one per frame.

## Migration Plan

Host-only. Unset `HMBTN_DUMP_TEXTURES` and the present path collects nothing. Rollback is reverting the host files. No save or disc change.

## Open Questions

None. The file name is the page and UV bounds the mods loader already matches, and a stored zero stays black.
