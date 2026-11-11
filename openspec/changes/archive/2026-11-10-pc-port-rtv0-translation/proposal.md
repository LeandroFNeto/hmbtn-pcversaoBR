# Proposal

## Why

The shipping bin, the chest and its lid, the doors, and the bed did not appear, or sat in the wrong place. Their models are drawn by `scene_spin_entries`, which builds each model matrix in `model_compose_gte_matrix` and ends with `gte_rtv0` to transform the object position. The game's own `src/include/gte.h` encodes `gte_rtv0` as `0x4A480012`, rotation times V0 plus TR. The host replaces that header, and its `gte_h_rtv0` computed rotation times V0 only. The camera translation was dropped, the object fell behind the camera, `rtpt` flagged it, and the model walker skipped every polygon (`drawn 0`, `nclip_fail 0`, `nclip_pass 0`).

## What Changes

- `gte_h_rtv0` adds `TR * 4096` before the `>> 12`, as the game's command does. `gte_h_rtir` stays without translation (`0x4A49E012`).
- The bed quad (`force_bed`), the forced `nclip` and `FLAG` writes it needed, and the `model.c` patch in `cmake/host.cmake` are removed. The `nclip` stays the PS1 cross product.

## Capabilities

### New Capabilities

- None.

### Modified Capabilities

- `host-gte-library`: the vector transform with translation matches the PS1 command.

## Impact

- `src/pc_port/src/gte.c`, `libgte_math.c`, `model_tex.c`, `cmake/host.cmake`, `force_props_check.c`; `force_bed.c` and `force_bed.h` deleted.
- Out of scope: the color forced on the bin and door holes (`libgpu.c`), the flashback background, the photo album.
