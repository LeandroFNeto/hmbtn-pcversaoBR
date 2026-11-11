# Proposal

## Why

The house and the childhood-flashback background are still solid black. The flashback that was on screen (`place 54`, `scene 0xe6`) read back black across the frame while the primitive over the center was an untextured quad of `rgb 255,255,255`. That line did not record the semi-transparency bit or the blend rate, and the existing mode-2 check uses a black source, which looks the same under addition and under subtraction.

## What Changes

- Semi-transparency rate 2 is measured with a white source over a known pixel. A readback that is the sum, or the destination left unchanged, becomes destination minus source. A readback that is already the difference is left as it is. No other rate is given a second blend, and no color is invented.
- One F8 on a house scene (`0x50`–`0x5B`) and one on the flashback (`place 54`, `scene 0xe6` or `0xe7`) record, at five points, the last primitive that covers each point, with its command, raw color, semi bit, and blend rate. The same frame records each `ClearImage` and each mask command. The record does not paint a replacement.
- The requirement that those regions show scenery stays unmet until a later check says the rectangle is gone. `pc-port-scenery-fixes` is not edited.

## Capabilities

### New Capabilities

- None.

### Modified Capabilities

- `gpu-packet-replay`: Rate 2 subtracts the source from the destination when a white-source measurement says the host is adding or leaving the destination unchanged. A house frame and a flashback frame record the primitive at each screen edge, the clears, and the mask words, without replacing those pixels.

## Impact

- Host diagnostic in `src/pc_port/src/libgpu.c`. `ClearImage` is observed through a linker wrap so the rectangle is logged without editing `src/pc_port/src/vram.c`. The wrap is a link option in `cmake/host.cmake`.
- No edits under `src/main`, `src/overlay`, or `src/include`. `glColorMask` and `glStencilMask` are not called. Draw-area commands `0xE3`, `0xE4`, and `0xE5`, and VRAM fill command `0x02`, are recorded and not implemented.
- Out of scope: the bed's backface test, the ground left of the farmhouse, the shipping bin, the door, a texture-mod file, and a sepia color.
