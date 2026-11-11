# Design

## Context

See proposal.md for why. `texture_resolve` is in `src/pc_port/src/vram.c`. `resolve_texture` in `src/pc_port/src/libgpu.c` calls it with the primitive's page, depth, CLUT, and UV bounds, then ignores the return. `flush_verts` always binds `gpu_vram_tex`. The fragment shader reads that atlas with `vram_word` at `page + uv`. `TextureSource` has one value, `TEXTURE_FROM_VRAM` (0). The host links SDL2 and does not link SDL_image.

## Goals / Non-Goals

**Goals:**

- Load one BMP per page and UV bounds, and sample it on the primitive that already carries those bounds.
- Leave every other primitive on the VRAM atlas.
- Keep the VRAM words and the disc read as they are.

**Non-Goals:**

- PNG, and a second decoder.
- Looking up an item id, or reading `picture_cells` to invent a page or UV.
- The shipping bin, the chest, the doors, and the unmet scenery requirement in `gpu-packet-replay`.
- Using `vram_observe_write` to hash `LoadImage` payloads. That seam stays a no-op.

## Decisions

1. **The key is the arguments `texture_resolve` already receives.** A file named `pageX_pageY_u0_v0_u1_v1.bmp` matches page `(pageX, pageY)` and UV bounds `(u0, v0)`–`(u1, v1)`. Depth and CLUT are not part of the key. An item id is not available on this call. The folder is `mods/textures` beside the executable (`SDL_GetBasePath`), which is `build/host/mods/textures` for the normal launch. `HMBTN_MODS` replaces that folder. The scan runs on the first resolve, and `mods_rescan` runs again so the self-check can write its BMP after the GL context exists.

2. **BMP only, through `SDL_LoadBMP`.** A file that does not end in `.bmp`, or that `SDL_LoadBMP` rejects, is skipped and logged. That key then has no match. PNG was the other format in the request. It stays out because the link line has no PNG decoder.

3. **The return stays a single unsigned value.** `0` remains `TEXTURE_FROM_VRAM`. A match returns the `GLuint` from `glGenTextures`, which is not 0. The function signature is unchanged, so the existing counting resolver still compiles. `libgpu.c` reads the return it currently discards. Two files with the same key are both dropped and the return is 0.

4. **A different name flushes the batch.** Vertices for a replacement are not drawn with `gpu_vram_tex`. The fragment shader gains one uniform. At 0 it keeps `vram_word`. At 1 it samples the bound image with UV remapped from the primitive's bounds onto the whole file. A bound with `u1 == u0` or `v1 == v0` samples the center of the file. The next VRAM primitive binds `gpu_vram_tex` again. The replacement is `GL_RGB`, nearest, clamp. Nothing is written into `vram[][]`. `gpu_gl_shutdown` deletes the names this change created.

5. **The self-check builds the square.** It writes an 8×8 24-bit red BMP under the key it will draw, calls `mods_rescan`, draws that primitive, and reads red. A second primitive with another UV samples a VRAM word of a different color. The red primitive's VRAM address is still the word it held before the draw.

## Risks / Trade-offs

- [The shader still samples the atlas after a bind] → Decision 4's uniform is set for the whole batch, and the batch flushes when the name changes.
- [A BMP larger than the sprite is stretched across the UV bounds] → That is the overlay. This change does not invent a second UV layout.
- [The first resolve scans before the self-check writes the file] → Decision 1's `mods_rescan` runs after the write.
- [A real portrait has no file yet] → The check uses a primitive whose page and UV are the file name. A portrait file is the same match once its page and UV are known. This change does not search for them.

## Migration Plan

Host-only. An empty `mods/textures` folder leaves every primitive on VRAM. Rollback is removing the folder and the host files. No save or disc change.

## Open Questions

None. The file name is the page and UV bounds already passed to `texture_resolve`.
