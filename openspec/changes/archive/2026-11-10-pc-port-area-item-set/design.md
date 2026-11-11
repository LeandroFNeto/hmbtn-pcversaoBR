# Design

## Context

See proposal.md for why. The requirements are the two ADDED blocks in `specs/host-area-item-set/spec.md`.

What the code does today:

- `scene_enter` stores `farm_area.mode.place` (`LimitPair`: `.value` is the map, `.location` is the area) and, when `resourceSet` changes, calls `farm_load_item_art`. That loads one pack and `item_art_bind_sprites` binds every slot in `farm_item_tocs` for that set.
- `picture_load_to_vram` copies one id into an 8-slot picture grid, and only when the id is in `resource_lists[resourceSet]`. Id `100` (`0x064`, the shipping-bin picture) is in none of the ten lists.
- `mark_forced_hole` in `src/pc_port/src/libgpu.c` sets the corner-color flag for any primitive on page `320,0` whose UV rectangle contains `(246, 242)`, and any primitive on page `384,0` whose UV rectangle contains `(222, 159)`. It does not read the place. The corner color already measured for the bin is `128,128,128`.

## Goals / Non-Goals

**Goals:**

- Arm the bin stand-in only for `PLACE_FARM` (`1`), `PLACE_4` (`4`, the coop), and `PLACE_5` (`5`, the barn). Arm the door stand-in only for `PLACE_FARM`.
- Match the primitive's own UV rectangle, so a larger rectangle that only contains the measured point is left discarded.

**Non-Goals:**

- Editing `place_entries`, `farm_item_tocs`, or inserting id `100` into a pack that has no pixels for it.
- Calling `picture_load_to_vram` for every id when the area loads. The picture grid has eight slots and the game fills them when an item is shown.
- The bed, the chest, the ground left of the house, and the flashback background.

## Decisions

### 1. The active area is the place the scene already stored

The host reads `farm_area.mode.place.location` when it decides the flag. `scene_enter` already writes that field. No patch on `scene.c`.

Alternative considered: wrap `scene_enter` and keep a second copy of the place. Rejected. The field is the area the rest of the farm already uses.

### 2. One rectangle per item, not a point inside a rectangle

The host table is one row per item: the areas, the page, and the UV min and max. A primitive matches when the page is equal and its UV min and max are equal to that row. `uv_contains` stops being the match.

The bin row is page `320,0`, UV `(240, 236)`–`(252, 248)`. The door row is page `384,0`, UV `(216, 153)`–`(228, 165)`. Both are the rectangles the current self-check already draws, and both contain the measured points. A self-check rectangle that contains `(246, 242)` but is wider than the bin row does not match.

Alternative considered: keep "contains the point" and add only the place test. Rejected. The tree is on the same page in the same area, so the place test would still paint it.

### 3. The proof is the self-check, then one farm view

`HMBTN_SELFTEST=1` sets `farm_area.mode.place.location`, draws the bin rectangle and the wider neighbor, and reads the pixels back. It restores the place it found. The farm view is one frame after that. If the logged polygon that contains `(246, 242)` is not the bin row, the capture task stays open and the row is not widened to cover the tree.

## Risks / Trade-offs

- [The game's bin polygon uses a different UV than `(240, 236)`–`(252, 248)`] → The capture task stays open and the row is not edited to the tree's rectangle. The self-check still locks the two constructed rectangles.
- [Coop and barn do not draw page `320,0`] → The place test still arms the row there. A polygon that is not the bin rectangle is not painted.
- [The gray the player sees is one quad covering the bin and the tree on screen] → That quad's UV is not the bin row, so this change leaves it discarded. Splitting one quad is out of scope.

## Migration Plan

Rebuild the host. The disc image and `mcd001.mcr` stay as they are. Rollback is restoring the point test in `mark_forced_hole` and removing the host table.

## Open Questions

None. A logged polygon that is not the recorded rectangle stops the capture task. That stop is the task's outcome, not a second rectangle.
