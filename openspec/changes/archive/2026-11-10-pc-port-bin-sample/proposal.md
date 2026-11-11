# Proposal

## Why

The shipping bin's textured primitive is command `0x2c`, page `320,0`, UV `246,242`. At 4 bits that is VRAM `(381, 242)`, inside the `LoadImage` at `(368, 242)` of size `14×1`, and the word there is `0x0000` (index 0). The disc picture for id `0x064` is an `.ANI` (`magic 0xE`) whose bytes are not all zero, and `item_art_address` still returns null for that id. This change asks whether those non-zero bytes were stored at a different address than `(381, 242)`.

## What Changes

- A host log names the sample address of that primitive and each `LoadImage` of the `0x064` picture, with whether the stored row contains a non-zero word.
- Exactly one non-zero row that does not cover `(381, 242)` supplies one page delta, or exactly one non-zero word inside a row that does cover it supplies one UV delta. The host sampler applies that delta only to command `0x2c` with page `320,0` and UV `246,242`.
- Any other count of non-zero rows or words stops the change. The lines are pasted. No color is invented.
- The in-memory lookup of `0x064` stays null. The disc read stays the same sector.

## Capabilities

### New Capabilities

- `host-bin-sample`: The shipping-bin primitive samples the VRAM address where its one non-zero picture word was stored, or it keeps sampling `(381, 242)` when no single delta is named.

### Modified Capabilities

- None. `item_art_address` still returns null for an absent id. The unmet scenery requirement in `gpu-packet-replay` stays there.

## Impact

- `src/pc_port`: the address log, the self-check, and the sampler delta when one line names it. The log stays `build/host/hmbtn-host.log`.
- No edits under `src/main`, `src/overlay`, or `src/include`. No host copy of `items.h` or `anim.h`.
- Out of scope: a non-null lookup, a second primitive, the chest, the house, inventing a texel, and the Linux build.
