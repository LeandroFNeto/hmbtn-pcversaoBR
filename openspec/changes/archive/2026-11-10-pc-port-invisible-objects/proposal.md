# Proposal

## Why

The chest, the shipping bin, and the doors of the house and the farm buildings are missing on the farm, and the same objects leave black shapes in the childhood flashback. `pc-port-gpu-fixes` archived with those holes still open: its trace did not match a texture window, a sample of word 0 whose tile lives elsewhere, or a line or fill sharing a packet.

## What Changes

- With `HMBTN_OBJECTS=1`, F8 dumps the frame the operator is looking at. One press in front of the shipping bin and one press in front of the chest each dump the `DrawOTag` queue for that object's screen rectangle: every command `0x20`–`0x7F`, whether the walker drew it or skipped it, and the GTE cull counts for that frame.
- The same dump records the `GsSetFlatLight`, `GsSetAmbient`, and `GsSetLightMode` arguments against the corner colors of any packet that covers the rectangle, so a stored black light is separated from the black clear.
- The branch that dump selects is the fix. After it, those objects are visible in the current scene, and the flashback no longer shows them as solid black rectangles.
- The earlier three regions (left of the house, the bed, the flashback background) stay unmet until a capture of this change shows they are these objects. This change does not add a fourth theory to the archived trace.

## Capabilities

### New Capabilities

- None.

### Modified Capabilities

- `gpu-packet-replay`: The chest, the shipping bin, and the house and farm doors are drawn, and the flashback does not show those objects as solid black. The existing requirement "The farm and the flashback show their scenery" stays as written and is still unmet for the bed and the side of the house.

## Impact

- Host GPU walk (`src/pc_port/src/libgpu.c`), the GTE (`src/pc_port/src/gte.c`), and the Gs light stubs (`src/pc_port/src/libgs.c`). The farm calls `GsSetFlatLight` three times and then `GsSetAmbient(0x800, 0x800, 0x800)` and `GsSetLightMode(0)`; the stubs ignore those arguments. `model_set_light_dir` is never called, so the light and color matrices stay cleared.
- `model_draw_tmd_object` links only `*_RAW` TMD headers. Any other header, including the lit codes and the flag-4 codes, makes that pass give up the rest of the object before a packet is linked. `model_quad_clip_check` always returns 0, and nothing calls it.
- No edit under `src/main/`, `src/overlay/`, or `src/include/`. A branch that has to change the TMD pass does it with `hmbtn_host_patch`, as the other host patches do.
- The archived scenery requirement in `gpu-packet-replay` already says the three old regions are not black. That text was synced while the picture still had the holes. This change does not rewrite it.
