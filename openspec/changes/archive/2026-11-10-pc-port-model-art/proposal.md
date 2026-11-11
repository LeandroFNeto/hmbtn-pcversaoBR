# Proposal

## Why

The farm objects that sample an empty word stay black, and filling them by a new vertex calculation would have to be repeated for every later model. The game already builds the character and the objects as TMDs, and it already knows how to place one item's picture. Future art, including a later 3D mesh, has to hang off that model instead of off a new formula.

## What Changes

- A look rule binds one model to one item picture, on a set of places, and only for one page and UV. The character and an object use the same rule. The vertices stay the ones that model already produced.
- The picture is the one `picture_load_to_vram` already loads. No texel is invented. If that picture has no non-zero pixel, the polygon keeps the sample the model calculated.
- A model with no rule, and a polygon of a ruled model that is not the rule's page and UV, keep the sample the model calculated.
- A BMP that already matches the polygon still replaces the sample. The rule runs only when that lookup does not.
- The first object rule is the shipping bin's picture, id `0x064`, on the farm, the coop, and the barn. Its model address and UV stay empty until a farm log names them, so the in-game bin is not retargeted by guesswork.
- The character is the same kind of rule. This change proves it with a constructed model and a constructed picture. It does not pick a live outfit.

## Capabilities

### New Capabilities

- `host-model-art`: A model keeps the vertices the game built. A look rule points one of its polygons at an item picture. A model with no rule keeps the sample it calculated.

### Modified Capabilities

## Impact

- Host only: one rule table in `src/pc_port`, read when a textured primitive is drawn, using the TMD `model_tex_lookup` already stored for that packet. `src/main`, `src/overlay`, and `src/include` are not edited. `model.c` is not patched. `model_draw_tmd_object` stays the vertex path.
- `host-texture-mods` stays as it is: a matching BMP still wins, and a primitive with no rule still samples VRAM. The picture is a later sample, only for a rule match. That is narrower than "a BMP miss always samples VRAM", and it applies only to the ruled polygon.
- Out of scope: a new mesh file, seasonal clothes as files, the farm door, the bed, the chest, the ground left of the house, and the flashback. Those become later rules on this table, not a new generator. The corner-color rectangle in `host-area-item-set` is not widened. `pc-port-bin-picture` still owns the in-game bin capture; this change does not fill that UV.
