# Proposal

## Why

A replacement BMP has to be named `pageX_pageY_u0_v0_u1_v1.bmp` before `texture_resolve` will use it. Nothing in the host writes that file from the primitive that is already on screen, so the page and UV of a broken sprite have to be copied by hand.

## What Changes

- With `HMBTN_DUMP_TEXTURES=1`, one rising F8 writes the textured primitives of that frame as uncompressed BMPs. Holding F8 does not write the next frame. The variable alone does not write every frame.
- Each file is named with that primitive's page and UV bounds, and each pixel is the color already stored for that texel, CLUT included. A stored `0x0000` stays black. No replacement color is invented.
- The files go in `mods/textures/dump/` beside the executable. The mods loader does not read that subfolder.
- A self-check builds one primitive, writes its BMP, and reads the stored color back.

## Capabilities

### New Capabilities

- `host-texture-dump`: One armed frame writes each textured primitive's stored texels to a BMP whose name is the page and UV bounds the mods loader already matches.

### Modified Capabilities

- None. `host-texture-mods` still loads only a BMP that is the single match in the mods folder. A dump file is not loaded until it is moved there.

## Impact

- `src/pc_port`: the same rising F8 that `host_present` already passes to the other dumps, a writer that reads `vram[][]` with the 4-bit, 8-bit, and 15-bit address those samplers already use, and `SDL_SaveBMP`. The log stays `build/host/hmbtn-host.log`.
- No edits under `src/main`, `src/overlay`, or `src/include`. The VRAM transfers, the disc fallback, and the mods match stay as they are.
- Out of scope: painting the shipping bin, moving a page or UV, the bed, the side of the house, PNG, and a dump on every frame.
