# Design

## Context

See proposal.md for why. The archived trace in `pc-port-gpu-fixes` task 4.2 is the starting record. Left of the house, the center was sometimes `cmd 0x2a` untextured `rgb 20,0,0` and sometimes `cmd 0x64` page `960,0` depth 8 with word `0x0000`. When the quad walked off the center, the black-blob count went to 0. The bed center `(254,170)` was covered by no primitive. The flashback center was covered by `cmd 0x2a` untextured `rgb 107,107,107`, and the black-blob count on that scene was 0. `e2`, `line`, and `fill` stayed 0 on all three.

`cmd 0x2a` is the flat untextured quad with the semi-transparency bit set. `set_texture_state` already stores that bit as `mode` for an untextured packet, and the fragment shader already changes the output when `mode` is not `-1`. What is not known is whether a constructed packet of `rgb 20,0,0` over a bright pixel reads back as the blend or as the opaque color. The host does not enable face culling. A back face is rejected by `gte_h_nclip` before a packet exists: `mac0 <= 0` fails, `mac0 > 0` passes. The window clear is `gpu_clear_screen`, which `gpu_put_draw_env` calls when the draw env asks for a background. `SetBackColor` stores a GTE register and does not paint the window.

## Goals / Non-Goals

**Goals:**

- Measure the untextured semi quad, and change `libgpu.c` only when that pixel is the opaque packet color.
- Say, for one bed frame, whether a group at the center failed the backface test, passed it, or never entered.
- Say, for one flashback frame, which clear and which quad the background is.

**Non-Goals:**

- A second blend formula when the constructed quad already blends.
- Recoloring the 8-bit sprite of word `0x0000`, or painting a sepia the trace did not store.
- Editing `scene.c` or any file under `src/main`, `src/overlay`, or `src/include`.

## Decisions

1. **The quad is measured before it is changed.** `HMBTN_SELFTEST=1` draws one `cmd 0x2a` of `rgb 20,0,0` over a known bright pixel, and the same corners with the semi bit clear. The semi readback is the blend. The clear-bit readback is `rgb 20,0,0`. If the semi readback is already the blend, the task stops and the shader is not given another blend. If it is the opaque color, the fix stays in `libgpu.c` and the same check is run again. The 8-bit sprite at the farm center is not given a color. A later capture of the left of the house is its own task: if that center is still a solid black rectangle, the task stays open and the line is pasted.

2. **The bed is a host backface question, not a scene edit.** One frame inside the house, with the existing object trace, logs the group nearest the bed center: screen point, `nclip_fail`, `nclip_pass`, and whether it was drawn. The self-check feeds `gte_h_nclip` a known front face and a known back face. A reversed sign is corrected in `gte.c` and the check is run again. A group that never enters, or a drawn primitive that misses the center, stops the task with the log pasted. `scene.c` is not edited. The capture of the bed place is a separate task and stays open when this one stops.

3. **The flashback log names the clear and the covering quad.** One frame of the childhood flashback logs the clear color the frame started from, the command and `rgb` of the untextured quad over the background center, its semi bit, and a `MoveImage` that covers the draw buffer. The clear compared here is the one `gpu_clear_screen` used, not the GTE back-color register. If that clear is not the color the game stored for the draw env, `libgpu.c` starts the frame from the stored color and the check is run again. If the center is the recorded `cmd 0x2a` of `rgb 107,107,107` and the clear matches, the task stops and the pixel stays that color. No sepia is invented. The capture of the background is a separate task and stays open when this one stops.

## Risks / Trade-offs

- [The farm hole is the 8-bit sprite, and the `rgb 20,0,0` quad is only sometimes on the center] → Decision 1 does not recolor that sprite. The farm capture stays open and the scenery requirement stays unmet for that view.
- [The bed group never enters] → The log says so. The fix is not a cull change and not an edit of `scene.c`.
- [The flashback gray quad is the effect the game asked for] → The pixel stays `rgb 107,107,107`. A black rectangle elsewhere in that scene is a later capture, not a new color.

## Migration Plan

Host-only. Rollback is reverting `libgpu.c` and, if the sign was changed, `gte.c`. No save or disc change.

## Open Questions

None. The three views are checked separately, and a view whose measurement stops is left as it was.
