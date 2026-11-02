# Proposal

## Why

The farm ground draws, and the shipping bin, the chest, and the doors do not. Those three are dynamic TMD instances (`scene_spin_entries` calls `model_draw`), not the static mesh and not a prerendered background. A primitive that `model_draw_tmd_object` rejects never reaches `DrawOTag`, and the existing `HMBTN_TRACE` log only describes black blobs of primitives that were already emitted, once every 60 vblanks. Nothing in that log can be aimed at the single frame where the player is standing in front of the bin, so the three host gaps that could hide the object stay guesses.

## What Changes

- One presented frame, armed by the operator and fired while standing at the shipping bin, writes a log of that frame's `DrawOTag` walk and of the libgte calls that feed it. The log names which of the three suspects the numbers match, or that none does.
- The three suspects, measured rather than assumed: an emitted primitive whose color is (0, 0, 0) or whose sample is word 0; a GTE discard (`stflg` or `nclip`) before the primitive is linked; a line (`0x40`–`0x5F`) or a mask-bit command (`0xE6`) the walker does not draw. An object whose screen test skips `model_draw` entirely is recorded as its own outcome, so it is not mistaken for a backface cull.
- `GsSetFlatLight` and `GsSetAmbient` stay empty of lighting behavior. The dump records the values they were given. `model_draw_tmd_object` copies TMD color words into the packet, and the game never issues `ncs` or `dpcs`, so an empty light stub is a match only when the emitted packet itself is black.
- No pixel changes, no change to GTE results, and no edits under `src/main`, `src/overlay`, or `src/include`. The fix for whichever suspect matches is a later change.

## Capabilities

### New Capabilities

- `host-object-trace`: One armed frame writes a log that separates a black emitted primitive, a GTE discard before the ordering table, a skipped line or mask command, and an object that never entered `model_draw`.

### Modified Capabilities

- None. `DrawOTag` keeps drawing what it draws today, and libgte keeps returning the same values. The dump is new behavior.

## Impact

- `src/pc_port/src/libgpu.c` (one-frame packet histogram and the primitives covering the view center), `gte.c` and `libgte_math.c` (counters on `rtps`/`rtpt`, `stflg`, `nclip`, `stopz`, and the matrix stack, only while a dump is armed), `libgs.c` (record the light arguments), `host.c` (the arming switch and the key).
- The log stays `build/host/hmbtn-host.log`. `HMBTN_SELFTEST=1` checks the classifier and a synthetic ordering table. The live check is one capture at the shipping bin, pasted into the task, with no fix applied.
- Out of scope: implementing flat lights, flipping culling, drawing lines, honoring the mask bit, a capture at the chest or a door, the Linux build, and mods.
