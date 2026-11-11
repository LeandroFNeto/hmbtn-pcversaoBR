# Proposal

## Why

The shipping bin and a house item draw as a transparent hole: the polygon is issued, and the texel it samples is the word `0x0000` written by a 14×1 `LoadImage`. The item-art log already named the lookup exit. `0x064` is `exit absent` with `list_count 35`, `count 35`, and `start 0`. `0x11e` is the same exit with `list_count 21`, `count 21`, and `start 0`. The table words match, and the id is not in `resource_lists`. Another flexible-array patch of `anim.h` cannot make `item_art_address` return a pointer for those ids.

## What Changes

- A host self-check under `HMBTN_SELFTEST=1` runs the picture upload on two constructed `.ANI` files (magic `0xE`): one pixel non-zero, one pixel zero. The non-zero pixel stays a non-zero VRAM word. The zero pixel stays `0x0000`. No color is invented.
- The same check reads the disc sector `picture_load_to_vram` already uses for id `0x064` (`0x3C19000 + (0x064 << 11)`). It records the first word and whether the image bytes are zero.
- When that sector is an `.ANI` with a non-zero pixel and the farm sample is still `0x0000`, the host upload that writes the 14×1 row keeps the source pixel. `src/main`, `src/overlay`, and `src/include` stay unmodified.
- When the sector is not an `.ANI`, or its image bytes are zero, the change stops and pastes those words. No struct is rewritten and no texel is filled in.
- The farm capture of the shipping bin is the picture check, and only after a non-zero source. A black rectangle on a zero source does not fail the build.

## Capabilities

### New Capabilities

- `host-item-picture`: The disc picture used when an item-art lookup misses keeps a non-zero source pixel in the VRAM word the upload writes, and keeps a zero source as `0x0000`.

### Modified Capabilities

- None. `item_art_address` still returns null when the id is absent. `gpu-packet-replay` still draws the packet it already draws. The unmet scenery requirement (the side of the house, the bed, the flashback) stays on that spec.

## Impact

- `src/pc_port`: the self-check, and the upload fix only when the disc sector for `0x064` is a non-zero `.ANI`. The log stays `build/host/hmbtn-host.log`.
- `cmake/host.cmake` only if the fix is a `hmbtn_host_patch`. The copy under `src/` is not edited.
- Out of scope: a new `items.h` or `anim.h` in `src/pc_port/include`, treating `0x05E` as a chest (`ITEM_RED_GRASS`), requiring `item_art_address` to return a TMD, inventing a texel, MDEC, and the Linux build.
