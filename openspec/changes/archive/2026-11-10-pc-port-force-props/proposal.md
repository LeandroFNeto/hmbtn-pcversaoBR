# Proposal

## Why

The shipping bin, the farm door, and the bed are still missing, and that is what blocks looking at the farm. The bin and the door already reach `DrawOTag` as textured polygons whose sample word is `0x0000`, which the shader discards, so the polygon is a hole. The bed's group never enters `nclip` (`drawn 0`), so no polygon is submitted for it. A texture file can already cover a hole that has a primitive. The bed cannot be covered that way.

## What Changes

- A textured primitive on the two measured keys, with no mods BMP for that key, whose sample word is `0x0000`, is drawn with the corner color already stored in the packet. The fragment is not discarded. Every other `0x0000` sample stays discarded.
- The bed object, the one whose screen point is nearest `(254, 170)` inside the house and whose primitives are not submitted, is linked by the host when its header code already has a size in `model_keys` / `model_values`. The linked primitive is untextured and uses the color word stored in the TMD. A header with no size stops the bed work. The `nclip` sign is not flipped.
- A BMP that already matches the page and UV, or the TMD address, still replaces the sample. This change adds no manifest and no new image format.

## Capabilities

### New Capabilities

- `host-forced-props`: The shipping bin and the farm door show the packet's own corner color when their measured sample is `0x0000` and no mods BMP matches. The bed shows an untextured primitive when its header code already has a size in the model's key table.

### Modified Capabilities

## Impact

- Host draw: `src/pc_port/src/libgpu.c` (the discard of color word `0x0000`, guarded by the two measured keys). The self-check joins `HMBTN_SELFTEST=1`.
- Bed link: `cmake/host.cmake` (`hmbtn_host_patch` on `src/main/geometry/model.c`) and one host function under `src/pc_port/`. The patch diverts only the header code the house trace names, and only when `model_key_value` for that code is non-zero. `src/main`, `src/overlay`, and `src/include` are not edited in place.
- `mods.c` is unchanged. A matching BMP still wins over the corner color.
- The black scenery left of the house, the flashback background, and the chest stay on their existing changes. This change does not claim those pixels.
