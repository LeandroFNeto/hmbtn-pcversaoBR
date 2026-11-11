# Design

## Context

See proposal.md for why. The requirements are the ADDED blocks in `specs/host-model-art/spec.md`.

What the code does today:

- `model_draw` still builds every vertex. `model_tex_lookup` already returns the TMD header that built a packet, and the host logs that address once as `modeltex: tmd_........`.
- A BMP named for that address, or for the page and UV, replaces the sample in `texture_resolve`. A miss samples VRAM. The shipping bin's word there is `0x0000`, so the polygon is black.
- `picture_load_to_vram` places one item id in a picture cell. `pc-port-bin-picture` can point one page and UV at item `0x064`, but it does not know which model built the polygon, and its stored row is still empty.

## Goals / Non-Goals

**Goals:**

- One rule table for an object and for the character. A rule names the model, the places, one page and UV, and one item picture.
- Keep the vertices. Change only the sample of the matching polygon.

**Non-Goals:**

- A new mesh file, a new vertex path, or a patch of `model.c`.
- A live outfit for the character, or a stored address for the in-game bin, before a log names them.
- The farm door, the bed, the chest, the ground left of the house, and the flashback.

## Decisions

### 1. The rule names the model, not a screen point

A primitive matches when `model_tex_lookup` returns the rule's model and the primitive's page and UV equal the rule. Two models that emit the same page and UV stay apart. Until a log fills the model and the UV, that row matches nothing.

Alternative considered: match every polygon whose UV contains one measured point. Rejected. That is the test that painted the tree, and it cannot tell two models apart.

### 2. The picture is the art; the model is the shape

The host calls `picture_load_to_vram` with the rule's item id when the place becomes one that rule names, and not again until the place changes. The cell is the picture. If the call returns 0, or the cell's texels are all word `0x0000`, the rule stays off.

Alternative considered: rebuild the polygon as an untextured quad inside `model_draw_tmd_object`. Rejected. That calculation would be copied for every later model, which is the cost this change is avoiding. A later mesh file would be another source on this same rule, not a new draw path.

### 3. The character and the object are two rows

The object row is item `0x064` on the farm, the coop, and the barn. The character row has the same fields and its own picture. Both ship with an empty model and an empty UV. The self-check fills two constructed rows, each with the address of the header it draws, and two constructed cells of different non-zero colors.

### 4. A BMP still wins

`texture_resolve` runs before the rule. A matching BMP leaves the picture unused. A model with no rule is not redirected, so it still samples VRAM.

## Risks / Trade-offs

- [The logged address is a pointer from one session] → The row is stored only when a second draw of that model reports the same address. A different address leaves the task open. No address is guessed.
- [The disc picture for the id is empty] → The rule stays off. No texel is invented.
- [The logged polygon also covers the neighbor] → The UV is not stored and the task stays open.
- [host-texture-mods says a BMP miss samples VRAM] → That remains true for a model with no rule. Only a rule match, after the BMP lookup misses, samples the picture.

## Migration Plan

Rebuild the host. The disc image and the mods folder stay as they are. Rollback is removing the rule table. The vertex path is untouched.

## Open Questions

None. An empty picture, a polygon that is not the model alone, or an address that changes between draws stops the capture task. That stop is the task's outcome.
