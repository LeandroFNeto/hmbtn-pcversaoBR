# Design

## Context

See proposal.md for why. The requirements are the two ADDED blocks in `specs/host-bin-picture/spec.md`.

What the code does today:

- The farm bin is a map polygon. In `f8-003.bmp`, `f8-005.bmp`, and `f8-007.bmp` (`place 1`, `scene 0x0`) it is a black square. Its UV is not the corner-color row `(240, 236)`–`(252, 248)`, so `mark_forced_hole` does not paint it. The tree in `f8-005.bmp` and `f8-006.bmp` already has its own texture.
- `picture_load_to_vram` places one item id in a picture cell. For an id absent from `resource_lists`, it reads the disc sector `0x3C19000 + (id << 11)`. Id `0x064` is absent from every list, so that sector is the picture. `picture_cell_build_prim` queues the cell as a sprite. The model draw does not call it for the bin.
- `model_draw_tmd_object` still builds every polygon. This change does not patch it.

## Goals / Non-Goals

**Goals:**

- Load id `0x064` through `picture_load_to_vram` while `farm_area.mode.place.location` is `PLACE_FARM`, `PLACE_4`, or `PLACE_5`.
- Point only the logged bin polygon at that cell's page, UV, and CLUT. Leave its vertices alone.

**Non-Goals:**

- A new vertex path, a screen-fixed sprite, or a patch of `model.c`.
- Loading every id in the area's `resource_lists` row.
- Widening the corner-color rectangle in `host-area-item-set`.
- The farm door, the bed, the chest, the ground left of the house, and the flashback background.

## Decisions

### 1. One picture call, not a new model

The host calls `picture_load_to_vram(0x064)` when the place becomes the farm, the coop, or the barn, and not again until the place changes. The cell the call fills is the picture. If the call returns 0, or the cell's texels are all word `0x0000`, the redirect stays off.

Alternative considered: rewrite `model_draw_tmd_object` so the bin is an untextured quad. Rejected. That is the model-generation change this port is not taking.

### 2. The redirect matches the logged polygon only

The host stores one page and one UV min and max, filled from the farm log of the polygon that covers the black square. A primitive matches when its page and its own UV min and max equal that row. The tree's UV does not, so its sample stays the one the model calculated. Until the log exists, the row is empty and the game polygon is not redirected. The self-check uses a constructed row and a constructed cell with a known non-zero color.

Alternative considered: redirect every polygon whose UV contains `(246, 242)`. Rejected. That is the test that painted the tree.

### 3. A BMP still wins

`texture_resolve` runs before the redirect. A matching BMP leaves the picture unused.

## Risks / Trade-offs

- [The disc sector for `0x064` is empty] → The redirect stays off and the capture task stays open. No texel is invented.
- [The logged polygon is one quad that also covers the tree] → The row is not stored and the task stays open.
- [Coop and barn use a different UV] → They still load the picture. The redirect fires only for the logged UV, so a different polygon there stays as it is.

## Migration Plan

Rebuild the host. The disc image and `mcd001.mcr` stay as they are. Rollback is removing the picture call and the redirect.

## Open Questions

None. An empty picture file, or a logged polygon that is not the bin alone, stops the capture task. That stop is the task's outcome.
