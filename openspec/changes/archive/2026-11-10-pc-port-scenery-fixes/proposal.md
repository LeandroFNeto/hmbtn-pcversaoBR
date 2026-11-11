# Proposal

## Why

Three regions of the base picture are still solid black: the ground left of the farmhouse, the place of the bed, and the childhood-flashback background. The archived GPU trace did not match a texture window, a misplaced sample, or a line or fill sharing the packet, so those three views need their own checks.

## What Changes

- An untextured quad that has the semi-transparency bit set blends with the pixel already in the frame. A quad of `rgb 20,0,0` with that bit clear stays `rgb 20,0,0`. No other color is written in its place.
- One house frame records whether a projected group at the bed center was rejected by the backface test or never entered. A wrong sign in that test is corrected. The game's scene selection is left as it is.
- One flashback frame records the clear color, an untextured quad over the background, and a VRAM copy that covers the draw buffer. The record does not paint a replacement color. A frame that starts from a color other than the clear the game stored starts from the stored color.
- Each of the three views is checked on its own. A view whose check stops stays a solid-black gap on the existing scenery requirement.

## Capabilities

### New Capabilities

- None.

### Modified Capabilities

- `gpu-packet-replay`: An untextured semi-transparent quad blends. A front face at the bed is not dropped by the host, and a group that never reaches the bed center is recorded without editing the game's scene selection. A flashback frame records how its background is composed and does not replace that color.

## Impact

- `src/pc_port/src/libgpu.c` for the untextured blend and the clear, and `src/pc_port/src/gte.c` only if the backface sign is wrong. The existing frame trace and the object trace supply the three logs. The log stays `build/host/hmbtn-host.log`.
- No edits under `src/main`, `src/overlay`, or `src/include`.
- Out of scope: the shipping bin, the chest, the doors, a texture-mod file, and a color that the trace did not already store.
