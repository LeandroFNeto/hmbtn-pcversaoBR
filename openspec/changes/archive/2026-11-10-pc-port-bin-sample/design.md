# Design

## Context

`sample_parts` in `src/pc_port/src/tmd_vram.c` reads a 4-bit texel at `x = page_x + u / 4`, `y = page_y + v`. For the bin that is `320 + 246 / 4 = 381` and `y = 242`. The same formula is in `src/pc_port/src/libgpu.c`, which is the draw path. The covering `LoadImage` is `(368, 242, 14, 1)`. The word at `(381, 242)` is `0x0000` and the index is 0.

The disc sector of id `0x064` (`0x3C4B000` of `A_FILE.BIN`) starts with `0x0000000E` and is not all zeros. `texture_pack_load_at` already stores a constructed non-zero word and leaves a neighboring zero word at `0x0000`. This change compares the address the primitive samples with the address the picture actually stores.

## Goals / Non-Goals

**Goals:**

- Name `(381, 242)` for that one primitive, and name each stored row of the `0x064` picture as zero or non-zero.
- Apply one page delta or one UV delta in the host sampler, only for command `0x2c` with page `320,0` and UV `246,242`, when the log names exactly one candidate.
- Leave the sample where it is when there is no single candidate.

**Non-Goals:**

- Returning non-null from `item_art_address`.
- Changing the disc sector, inventing a texel, or moving any other primitive.
- The chest, the house, or the scenery requirement in `gpu-packet-replay`.

## Decisions

1. **The address check is a constructed note.** The self-check builds page `320,0`, UV `246,242`, depth 4, and expects x `381`, y `242`. A separate `0x0D` row stores `0x7FFF` at a different rectangle and is logged as non-zero and not covering `(381, 242)`. A zero word stays `0x0000`.

2. **The picture rows come from the sector.** The self-check reads the same sector as `pc-port-dynamic-objects-fix` and uploads each `0x0D` pack in it through `texture_pack_load_at`, using the coordinates stored in the pack. Each row is one line: rectangle, and `zero` or `nonzero`. The sample line is `itemaddr: sample x 381 y 242`. No play session is required for this comparison.

3. **One candidate moves only that primitive.** One non-zero row that does not cover `(381, 242)` adds its delta to `page_x` and `page_y` in the host sampler, and only when the command is `0x2c`, the page is `320,0`, and the UV is `246,242`. One non-zero word inside a covering row whose sample pixel is zero moves `u` and `v` by the delta to that word, under the same guard. A self-check with one such row reads the stored non-zero word afterward. Any other count pastes the lines and stops. No second theory inside this change.

4. **The lookup is not part of this change.** No task calls `item_art_address` for a non-null result. The spring list still misses `0x064`.

## Risks / Trade-offs

- Command `0x2c` is used by other objects. The guard is the page and the UV together, so a different `0x2c` is left alone.
- A pack may store several non-zero words. More than one candidate is a stop, not a guess.
- Uploading the sector's pack in the self-check writes VRAM. The check uses a rectangle that the farm sample does not already own, or it restores the words it wrote.

## Migration Plan

Host-only. Revert the sampler change and the log returns the previous draw. No save or disc format change.

## Open Questions

None. The stop condition is the same one `pc-port-dynamic-objects-fix` already used: a measured zero is not a color to invent.
