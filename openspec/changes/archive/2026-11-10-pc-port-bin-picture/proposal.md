# Proposal

## Why

The shipping bin on the farm is still a black square. Its map polygon samples an empty word, and the recorded corner-color rectangle is not that polygon, so the stand-in never fires. Rebuilding the game's model generation to fix one sprite is more change than this port can take. The game already knows how to place one item's picture.

## What Changes

- While the current area is the farm, the coop, or the barn, the host loads the shipping bin's existing item picture, id `0x064`, through the picture call the game already uses. The model draw is not replaced.
- The bin polygon keeps the vertices the model already produced. Its texture sample becomes that picture's page, UV, and CLUT. A neighboring polygon, including the tree, keeps the sample the model calculated.
- The picture comes from the item file the game would read for that id. No new texel is invented. If that file has no pixels, the bin stays undrawn and the task stops.
- A BMP that already matches the polygon's page and UV still replaces the sample.

## Capabilities

### New Capabilities

- `host-bin-picture`: The shipping bin shows the item picture the game already loads for id `0x064`, on the farm, the coop, and the barn, without a new model path. A neighbor on the same page keeps its calculated sample.

### Modified Capabilities

## Impact

- Host only: one call into the existing picture load, and one texture redirect in `src/pc_port/src/libgpu.c` for the logged bin polygon. `src/main`, `src/overlay`, and `src/include` are not edited. `model.c` is not patched.
- The corner-color stand-in in `host-area-item-set` stays for its recorded rectangle. This change does not widen that rectangle.
- The farm door, the bed, the chest, the ground left of the house, and the flashback background stay on their existing changes.
