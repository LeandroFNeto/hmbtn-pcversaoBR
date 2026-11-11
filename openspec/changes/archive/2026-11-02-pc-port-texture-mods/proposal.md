# Proposal

## Why

`texture_resolve` already runs once per textured primitive and always returns `TEXTURE_FROM_VRAM`. `libgpu.c` discards that return and binds the VRAM atlas. A replacement image has nowhere to attach, so a file on disk cannot cover one sprite without rewriting the game's pages.

## What Changes

- The host loads BMP files from a folder beside the executable and matches one file to the page and UV bounds the packet already passes to `texture_resolve`.
- A single match returns that image's OpenGL name. The draw splits the batch, binds that name, and samples the file across the primitive. The VRAM array is not written.
- No match, a failed file, or two files with the same key keep `TEXTURE_FROM_VRAM`.
- A self-check draws one primitive from a generated colored BMP and one primitive that still samples VRAM.

## Capabilities

### New Capabilities

- `host-texture-mods`: A textured primitive samples a BMP from the host mods folder when that file is the only match for the primitive's page and UV bounds, and samples VRAM otherwise.

### Modified Capabilities

- None. `gpu-packet-replay` stays as written. A missing mod does not change how a packet samples VRAM.

## Impact

- `src/pc_port`: `texture_resolve` in `vram.c` (called from `libgpu.c`, which today ignores the return), the batch bind in `libgpu.c`, and a self-check. The host already links SDL2, whose `SDL_LoadBMP` reads the files. SDL_image is not linked.
- No edits under `src/main`, `src/overlay`, or `src/include`. `vram_load`, `vram_move`, and `vram_fill` stay. The disc fallback stays.
- Out of scope: PNG, a portrait whose page and UV are not in a file name, seasonal clothes, the shipping bin, the chest, the doors, and any page or UV written by the game.
