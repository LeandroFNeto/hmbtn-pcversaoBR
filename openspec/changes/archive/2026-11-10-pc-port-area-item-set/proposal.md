# Proposal

## Why

Entering an area already loads that area's item-art set, but the gray stand-in on the shipping bin does not follow the set. It paints every polygon on page `320,0` whose UV rectangle contains `(246, 242)`, on every map. The next tree's rectangle also contains that point, so the farm shows one gray design on the bin and on the tree. The bin's picture id `100` is not in any `resource_lists` row, so the area set never had that image.

## What Changes

- The corner-color stand-in is armed only while the current area is one of the item's areas. The shipping bin is armed on the farm (`PLACE_FARM`), the coop (`PLACE_4`), and the barn (`PLACE_5`). The farm door is armed on the farm only. Leaving those areas disarms it.
- A polygon takes that color only when its page and its own UV rectangle are the item's recorded rectangle. A larger rectangle that merely contains the measured point does not match, so the tree stops using the bin's color.
- Entering the area is what arms the row. No extra per-item load is added. `picture_load_to_vram` stays as it is. `place_entries` and `farm_item_tocs` are not edited, and id `100` is not inserted into them, because the pack has no pixels for that id.
- A BMP that already matches the page and UV still replaces the sample.

## Capabilities

### New Capabilities

- `host-area-item-set`: The shipping bin and the farm door show the packet corner color only while their area is active, and only on their own UV rectangle. A neighbor that only contains the measured point stays discarded.

### Modified Capabilities

## Impact

- Host draw: `src/pc_port/src/libgpu.c`, where the stand-in is chosen, and one host table of item, areas, page, and UV rectangle. The self-check joins `HMBTN_SELFTEST=1` and reads `farm_area.mode.place.location`.
- `src/main`, `src/overlay`, and `src/include` are not edited. The area enter in `scene_enter` already stores the place and already calls `farm_load_item_art`.
- The bed, the chest, the ground left of the house, and the flashback background stay on their existing changes.
