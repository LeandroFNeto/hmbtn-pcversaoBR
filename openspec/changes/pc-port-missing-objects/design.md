# Design

## Context

See proposal.md for why. The requirements are in `specs/host-object-trace/spec.md`.

`scene_spin_entries` places each dynamic object with `PushMatrix`, projects its position with one `rtps` (`gte_project_vertex`), and skips `model_draw` when that point is outside the screen box (`x` from -100, width 521, `y` from -100 to 341). `model_draw` pushes again and `model_draw_tmd_object` transforms each primitive with `rtpt`, drops it when `stflg` is negative, drops it when `nclip`'s `stopz` is not positive, then links a polygon. The farm ground is a different call, `model_draw_visible`, one push for the whole mesh.

`DrawOTag` draws `0x20`–`0x3F` and `0x60`–`0x7F`. A line (`0x40`–`0x5F`) returns from the packet immediately, so later words in that packet are not read. `0xE6` is skipped a word at a time and does not change mask state. `HMBTN_TRACE` records emitted primitives and, every 60 vblanks on the farm, the black blobs. It never sees a primitive the GTE already dropped.

`GsSetFlatLight` and `GsSetAmbient` discard their arguments. `scene_set_lights` runs at scene setup, before anyone can press a key: light 0 is `(0xD0, 0xD0, 0xD0)`, lights 1 and 2 are black, ambient is `0x800`. Packet colors are copied from the TMD.

## Goals / Non-Goals

**Goals:**

- One F8 press after `HMBTN_OT_DUMP=1` writes the spec's log for the next presented frame, then stops.
- The outcome of each reported object comes from that object's own triangles, not from the ground pixel at the center.
- `HMBTN_SELFTEST=1` checks the walk counts, the light record, and the outcome rules without a play session.

**Non-Goals:**

- Changing a pixel, a GTE result, or a file under `src/main`, `src/overlay`, or `src/include`.
- Drawing lines, honoring the mask bit, or applying flat lights.
- A capture at the chest or a door. The same log format would serve, and this change does not ask for it.

## Decisions

### 1. Arm on F8, collect the next frame, then disarm

`HMBTN_OT_DUMP` is read once, the same way as `HMBTN_TRACE`. `host_present` polls `SDL_SCANCODE_F8`. A rising edge while the variable is `1` and no dump has been written clears the counters and collects from then until the following present. That present writes the log and ignores F8 for the rest of the process. Without the variable, F8 writes nothing. `gpu_trace_frame` is not called from this path and is not cleared by it.

The self-check does not press F8. A debug arm collects around a synthetic list and a synthetic push/project/transform/pop, then reads the same lines.

Alternative considered: dump the frame in which the key is noticed. Rejected: `host_present` runs after that frame's draws, so the counters would be empty.

### 2. Count the walk without drawing anything new

While collecting, `run_packet` increments four counters: polygons drawn (`0x20`–`0x3F`), rectangles drawn (`0x60`–`0x7F`), lines seen (`0x40`–`0x5F`), and every other command, of which `0xE6` is also its own counter. A line with words after its own length (3, 4, 5, or 6, as today) increments a cut counter. The line still returns immediately. Drawn primitives are stored in a static buffer used only while collecting: command, rgb, vertices, and the sampled word for a textured one. The view center is `(160, 120)`. The log line for it is the topmost stored primitive that contains it, or `uncovered`.

```
trace object: walk poly N rect N line N line_cut N mask N other N
trace object: center cmd 0xNN rgb R,G,B word 0xNNNN
trace object: center uncovered
```

No allocation in the walk. Overflow sets a flag on the walk line.

### 3. One group per scene-object push

`PushMatrix` opens a group. The first `rtps` in it stores the screen point. `rtpt` counts a primitive transform. An `stflg` that follows `rtpt` and is negative counts a front-of-camera failure (the projection's own `stflg` does not). `stopz` after that `rtpt` counts a failed backface when `mac0 <= 0` and a pass otherwise. A pass whose triangle contains `(160, 120)` keeps those three points. `PopMatrix` of a group with no screen point adds its counters to the parent: that is `model_draw`'s inner push, inside the entry that already projected. A group that has a screen point is complete.

The central half is `x` in `[80, 240)` and `y` in `[60, 180)`. Every complete group in that half is logged. If none are, the group nearest `(160, 120)` is logged instead. A fixed table of 64 groups is enough for the 15 scene entries plus the other projections in a frame; past that, the log says the table overflowed.

A primitive of the group covers the center only when a stored `DrawOTag` primitive has those three vertices. The ground's primitive fails that test, so it cannot choose the group's outcome.

```
trace object: group x X y Y rtpt N flag_fail N nclip_fail N nclip_pass N drawn N
```

`drawn` is how many of the group's passing triangles were found in `DrawOTag`. The counters are the raw evidence; the outcome line is only the name below.

### 4. One outcome, first match

For each reported group, the first rule that matches is the line:

1. `rtpt` is 0: `never entered its primitive pass`
2. `rtpt` is positive and `nclip_pass` is 0: `geometry discard`
3. One of its drawn primitives covers the center and is rgb `(0, 0, 0)` or word 0: `black emitted primitive`
4. `nclip_pass` is positive, none of its primitives cover the center, and the frame's line count or mask count is positive: `skipped line or mask command`
5. Otherwise: `none`

```
trace object: outcome <name> at X,Y
```

Rule 2 covers a front-of-camera failure as well as a backface, because both leave `nclip_pass` at 0. The group line still shows `flag_fail` and `nclip_fail` apart, so the paste says which test it was.

### 5. Remember the lights, and do not apply them

`GsSetFlatLight` copies the id's direction and color into a static slot of three. `GsSetAmbient` copies its three values. Both still return without lighting anything. The copy happens on every call, including scene setup before F8, and the dump prints the slots:

```
trace object: lights 0 R,G,B 1 R,G,B 2 R,G,B ambient R,G,B
```

The self-check sets light 0 to `(208, 208, 208)`, reads that slot back, draws a polygon of a different color, and reads the pixel. The pixel is the polygon's color.

### 6. The capture is the shipping bin, and it stops there

The operator starts a game, stands facing the shipping bin, runs with `HMBTN_OT_DUMP=1`, presses F8 once, and pastes the `trace object:` lines into the task. No branch edits the picture. A later change picks the fix from that paste. `git diff -- src/main src/overlay src/include` stays empty.

## Risks / Trade-offs

- [The player projects nearer the center than the bin] → Every central group is a separate line with its screen point. The paste is read by point, not collapsed to one object.
- [A HUD line shares the frame, so rule 4 names a group whose triangles simply miss `(160, 120)`] → The group line carries `nclip_pass` and `drawn`. The task records those numbers, and a later change does not treat "skipped command" as proven when `drawn` is positive.
- [`model_clip_test3` drops a passing `nclip` and the frame has no line] → The outcome is `none`, and the group line shows `nclip_pass` positive with `drawn` at 0. That gap stays visible instead of being forced into one of the three suspects.
- [The bin's triangles miss the single center pixel] → Rule 3 does not fire. The group line still has the counters. The center line separately names whatever primitive, ground included, covers `(160, 120)`.

## Migration Plan

The switch defaults off. Nothing in an existing save, log, or build flag changes until `HMBTN_OT_DUMP=1` is set.
