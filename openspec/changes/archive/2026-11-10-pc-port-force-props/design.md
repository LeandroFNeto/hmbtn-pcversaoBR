# Design

## Context

See proposal.md for why. The requirements are the two ADDED blocks in `specs/host-forced-props/spec.md`.

What the code does today:

- In `src/pc_port/src/libgpu.c` the fragment shader discards a textured sample when the color word is below `0.5`. Word `0x0000` is that case. The shipping bin is page `320,0` with a UV rectangle that contains `(246, 242)`. The farm door is page `384,0` with a UV rectangle that contains `(222, 159)`. Both samples were `0x0000`. The corner color on the drawn polygon was `128,128,128`.
- `texture_resolve` in `src/pc_port/src/vram.c` already returns a mods BMP before the VRAM sample. The shader takes that path when `uReplace` is set, and that path does not discard.
- `model_draw_tmd_object` in `src/main/geometry/model.c` links only `*_RAW` headers, in a fixed order. The last mismatch, against `TMD_PRIM_GT4_RAW`, goes to `done` and leaves the rest of that object unlinked. No `nclip` runs for those primitives. `object_trace.c` already names the house group nearest `(254, 170)` and its `drawn` count.
- `model_relocate_textures` already advances `TMD_PRIM_FT4` by the same `0x20` as `TMD_PRIM_FT4_RAW`, and the FT4_RAW draw loop already loads vertices at halfword offsets `0x18`, `0x1A`, `0x1C`, and `0x1E` and copies the color word at `source + 4`.

## Goals / Non-Goals

**Goals:**

- The two measured keys become visible from the packet corner color when the sample word is `0x0000` and no BMP matches.
- The bed group nearest `(254, 170)` is linked when its header has a RAW twin of the same stride already walked in `model_draw_tmd_object`.

**Non-Goals:**

- Flipping the `nclip` sign, or drawing a screen-fixed sprite.
- A new mods filename, a manifest, or a PNG loader.
- The chest, the ground left of the house, and the flashback background.
- A byte offset that `model_relocate_textures` does not already use for that header.

## Decisions

### 1. The empty sample is a per-primitive flag, not a global change

The walker sets one flag on a primitive whose page and UV rectangle contain `(246, 242)` on page `320,0`, or `(222, 159)` on page `384,0`, and whose `texture_resolve` result is not a BMP. In the shader, when that flag is set and the color word is below `0.5`, the fragment uses `vColor` and is not discarded. `stp` stays clear so the corner color is opaque. Any other `0x0000` sample still discards. A set `uReplace` still samples the BMP and ignores the flag.

Alternative considered: treat every `0x0000` as the corner color. Rejected, because index 0 is the transparent texel of ordinary sprites.

### 2. The bed reuses the RAW twin of the same stride

`hmbtn_host_patch` replaces the final `goto done` on a `TMD_PRIM_GT4_RAW` mismatch with a host call. The call runs only for the object `object_trace` would report as nearest `(254, 170)` while that object's `drawn` count is still 0. The header's low 16 bits must be a code `model_relocate_textures` advances by the same count as a `*_RAW` loop in `model_draw_tmd_object`. For that code the host repeats that loop's vertex loads and color copy, writes an untextured command (`0x28` for a quad, `0x20` for a triangle), and returns to the top of the ladder for the next primitive. `nclip` still rejects a non-positive result. A code with no same-stride RAW twin returns "stop", and the object ends as it does today.

`TMD_PRIM_FT4` is the pair that already matches: stride `0x20` with `TMD_PRIM_FT4_RAW`. A house trace whose header is not such a pair stops the bed task. No second code is added in this change.

Alternative considered: invent vertex offsets for every non-RAW code. Rejected. The relocate switch knows the stride and the texture fields, not a second vertex layout.

### 3. The proof is a constructed primitive, then one house view

`HMBTN_SELFTEST=1` builds a 4-bit primitive on page `320,0` whose UV contains `(246, 242)`, with color word `0x0000` and corner color `128,128,128`, and reads that corner color back. The same word on page `0,0` still leaves the cleared color. A BMP for the bin key still reads the BMP color. A one-primitive object headed `TMD_PRIM_FT4`, placed so its screen point is the nearest to `(254, 170)`, reads its stored color back. An object whose header has no same-stride RAW twin still reads the cleared color.

The house capture is one frame after that check. If the logged header is not a same-stride pair, the capture task stays open and the patch is not extended.

## Risks / Trade-offs

- [The door or the bin moves to another UV] → Only a rectangle that contains the measured point is forced. A different UV stays discarded, and the capture task stays open.
- [The nearest group is not the bed mesh] → The capture task stays open. A second object is not added.
- [Linking `TMD_PRIM_FT4` with the RAW vertex offsets draws a skewed quad] → The self-check uses a constructed quad with those offsets. A skewed house capture stops the task. The offsets are not edited.
- [The patch runs on every model] → The call returns immediately unless the object is the undrawn nearest group to `(254, 170)`.

## Migration Plan

Rebuild the host. The disc image and `mcd001.mcr` stay as they are. Rollback is reverting the shader flag, the host function, and the `hmbtn_host_patch` line.

## Open Questions

None. A header that is not a same-stride RAW twin stops the bed task. That stop is the task's outcome, not a second approach.
