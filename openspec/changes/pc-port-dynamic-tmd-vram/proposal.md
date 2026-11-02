# Proposal

## Why

The house frame dumped with `HMBTN_OBJECTS=1` draws the textured primitives of the scene (`0x2c`, `0x3c`, `0x64`, `0x66`) at `rgb 128,128,128` and samples word `0x0000`. That word is already zero in the host's VRAM mirror, so the polygon blends away into the black clear. `pc-port-invisible-objects` stops on branch E: the walker did not skip a polygon, the GTE passed 534 faces, and every farm header was `*_RAW`.

## What Changes

- One farm frame, with the chest or the shipping bin filling the middle of the view, records each textured primitive that samples word `0x0000` together with the `LoadImage`, `MoveImage`, and `ClearImage` rectangle that last wrote that VRAM address, and the texture page carried in the packet.
- The trace picks one fix. A rectangle that was issued and never stored is stored. A page that does not match the rectangle that holds the texels is corrected for the codes the trace names. A stored word that is already `0x0000` stops the change.
- After that fix, the chest, the shipping bin, and the house and farm doors are not transparent holes. The childhood flashback uses the same fix.

## Capabilities

### New Capabilities

- None.

### Modified Capabilities

- `gpu-packet-replay`: A textured primitive of the chest, the shipping bin, or a house or farm door samples a non-zero color word when the game loaded a non-zero texel at the address that packet names. The requirement "The farm and the flashback show their scenery" stays as written.

## Impact

- Host VRAM writes (`src/pc_port/src/vram.c`) and the textured-polygon walk (`src/pc_port/src/libgpu.c`). `sample_word` already reads the CPU mirror `vram[][]`, and `upload_vram` already sends dirty rows with `glTexSubImage2D`. A stale OpenGL texture is not the reading this log made.
- `0x3c` takes its texture page from the second vertex of the packet (`set_tpage` on that word). `model_relocate_textures` rewrites TMD pages in place and is not a VRAM copy.
- No edit under `src/main/`, `src/overlay/`, or `src/include/` unless the trace names a page adjust that has to live beside `model.c`, in which case it is a `hmbtn_host_patch`.
- `pc-port-invisible-objects` is not reopened. This change starts from the `word 0x0000` lines it already recorded.
