# Design

## Context

See proposal.md for why. The picture requirement is the two ADDED requirements in `specs/gpu-packet-replay/spec.md`.

What the code does today, as opposed to the guess that every missing object is dropped before `DrawOTag`:

- The walker draws polygon commands `0x20`–`0x3F` and rectangle commands `0x60`–`0x7F`. Bit 0 and bit 1 of those commands are the raw-texture and semi-transparency flags; both are passed through. A line command `0x40`–`0x5F` makes `run_packet` return without drawing and without walking the rest of that packet. The mask bit is not read.
- `model_draw_tmd_object` links a primitive only when its header matches a `*_RAW` code, in a fixed type order. Any other header, including the lit codes (`TMD_PRIM_FT4` is `0x2C00`, `TMD_PRIM_FT4_RAW` is `0x2D01`) and the flag-4 codes, ends that object's pass. No packet is linked for the rest of the object.
- Each linked primitive is still dropped when the GTE flag is negative, when `nclip` returns `<= 0`, or when the screen clip test fails. `model_draw_visible` also skips an object whose centre fails the four side planes. `gte_h_nclip` is implemented. The house is already on screen, so `nclip` is not inverted for every face.
- `GsSetFlatLight` returns `0` and ignores its pointer. It does not return a pointer. `GsSetAmbient` and `GsSetLightMode` ignore their arguments. `scene_set_lights` calls all three. The light and color matrices are cleared in `camera_init_projection` and nothing calls `model_set_light_dir`, so they stay clear. The `*_RAW` pass copies the color words out of the TMD; it does not read those matrices.
- The archived trace, on a real farm frame, found a covering primitive at the hole left of the house (`0x64` sampling word `0x0000`, and at times an untextured `0x2A` with corners `20,0,0`). The bed's centre had no primitive. `e2`, line, and fill counters stayed 0. `20,0,0` is a drawn color. It is not a light that returned zero, and it is above the black-pixel test (`R,G,B < 8`).
- `model_quad_clip_check` always returns 0. Nothing calls it.

The host image must still end below `0x1000000`. The dump allocates once. No allocation is added to the ordinary frame.

## Goals / Non-Goals

**Goals:**

- One structured frame in front of the shipping bin, and one in front of the chest, that says which of the four branches below is true.
- Apply that one branch so the spec's objects are visible in the current scene and are not solid black in the flashback.

**Non-Goals:**

- Lines, dithering, and the mask bit.
- `ncs`, `nccs`, and `dpcs`. The game's farm draw does not issue them.
- Recoloring a `*_RAW` packet whose corners are already non-zero.
- Calling `model_quad_clip_check` into service. Nothing calls it.
- Rewriting "The farm and the flashback show their scenery", or treating the bed and the side of the house as done because these objects became visible.
- An in-place edit of `src/main/`, `src/overlay/`, or `src/include/`.

## Decisions

### 1. F8 dumps the frame the operator is looking at

`HMBTN_OBJECTS=1` keeps the dump buffer, and it does not write a frame by itself. A rising edge of F8, on a farm frame, dumps that frame. The operator walks to the shipping bin, lines the camera up, presses F8 once, walks to the chest, and presses F8 again. Each press is its own dump in the same log. Holding the key does not dump the following frames.

A fade is a full-screen untextured polygon, which is what the archived captures saved. F8 on a fade writes nothing and does not use up the press: the next rising edge dumps the frame it lands on. Frames before the key are discarded, so loading the farm does not consume a dump.

The blob is the largest connected set of cells on the existing 4×4 grid whose read-back color has `R,G,B < 8`. The dump prints the blob's pixel bounds and centre. It reuses the row buffer the frame read-back already has. It does not add a static image.

### 2. The queue log names skips without assuming one

For every command `0x20`–`0x7F` whose rectangle overlaps the blob, the log prints the command, the four corners, the UV and the VRAM word when the command is textured, the corner color, and whether the walker drew it. A line `0x40`–`0x5F`, or any other command the walker steps over, is printed as skipped, with the following command byte in that packet when one exists. Bit 0 and bit 1 are printed with the command. They are not treated as a skip: the walker already draws those polygons.

The same frame prints three GTE totals from the host implementations: projections whose flag word is negative, `nclip` results `<= 0`, and `nclip` results `> 0`. It also prints, from the loaded farm model, each object's primitive-header counts split into `*_RAW` and anything else, and whether `model_draw_visible` would have rejected that object's centre. That last check is a host-side read of the centre and the four planes. It does not change the game function.

`GsSetFlatLight`, `GsSetAmbient`, and `GsSetLightMode` store their arguments in the host. The dump prints the three lights (id, direction, rgb), the ambient, and the mode. The return of `GsSetFlatLight` stays `0`.

### 3. One branch, chosen by that log

No branch is written before the log is in the task. Exactly one branch applies.

- **A. The walker skipped a polygon that overlaps the blob.** A `0x20`–`0x3F` or `0x60`–`0x7F` command was in the packet and the walker returned or stepped past it. The fix consumes that command's own words and continues the packet. Lines stay undrawn. The mask bit stays unread.
- **B. A drawn packet covers the blob with corners `0,0,0`, and the stored light is non-zero.** The fix keeps the stored lights and uses them only for a lit header that this change emits. A `*_RAW` packet that already has a non-zero color is left alone. A textured packet whose corners are non-zero and whose sample is word `0x0000` is the archived sample case: stop, do not recolor it.
- **C. Nothing covers the blob, and the GTE totals show the faces were submitted and then culled, or the centre projects inside the screen while the plane test rejects it.** The fix changes a host GTE result only when a fixed triangle in the self-test disagrees with the PS1 library. `nclip` is not flipped for every face.
- **D. Nothing covers the blob, the GTE totals show those faces were never submitted, and the object's headers are not `*_RAW`.** The type ladder gave up the object. The fix is `hmbtn_host_patch` on `src/main/geometry/model.c`: the failing compare calls a host function for the codes the log names. The byte size of each code is the size `model_relocate_textures` already advances for that code. The host function projects with `rtpt` / `nclip` / `avsz` and links a packet. A lit code uses the stored flat light and the vertex normal. A code the log names that has no size in that switch stops the branch; the header is pasted into the task and no layout is invented.
- **E. None of A–D.** Stop. The log is pasted into the task. No further theory is added.

### 4. The flashback uses the same branch

The flashback is not a separate cause. After the branch, a capture of the flashback is checked against the second requirement. If the farm objects are visible and the flashback still draws them black, the task stays open and the log of that flashback frame is pasted beside the farm log. A new branch is not opened for the flashback alone.

## Risks / Trade-offs

- [The dumped frame is a fade, as the archived bitmaps were] → F8 on a full-screen untextured polygon writes nothing. The next press dumps the frame it lands on.
- [A dark drawn color such as `20,0,0` is called a missing light] → Branch B requires corners `0,0,0` plus a non-zero stored light. `20,0,0` does not select it.
- [Branch D has to grow a large function] → The patch only diverts the codes the log names, and only when that code already has a size in the relocate switch.
- [The shipping bin and the chest are not the holes from the archived trace] → The spec's scenarios name these objects. The bed and the side of the house stay on the older requirement until a capture shows they are the same pixels.

## Migration Plan

Rebuild the host. Saves and the disc image are unchanged. Rollback is reverting the host files and, if branch D landed, the `hmbtn_host_patch` entry.

## Open Questions

None. The log picks the branch, and a log that matches none stops.
