# Tasks

## 1. Record the lights

- [x] 1.1 Copy each `GsSetFlatLight` and `GsSetAmbient` into static slots per design.md decision 5, and leave packet colors alone. Verify `HMBTN_SELFTEST=1` reads light 0 back as `(208, 208, 208)` and that a drawn polygon whose color differs from that light reads back as the packet color.

## 2. Ordering-table walk

- [x] 2.1 While a debug collection is active, count polygons, rectangles, lines, line cuts, mask commands, and other commands per design.md decision 2. The line still returns immediately. Verify the self-check walks one of each and the log reports `poly 1 rect 1 line 1 line_cut 1 mask 1`.
- [x] 2.2 Record the drawn primitives of that collection and the view center `(160, 120)`. Verify the self-check: an untextured primitive of color `(0, 0, 0)` covering the center logs that color, a textured one sampling word 0 logs `word 0x0000`, and an empty list logs `center uncovered`.

## 3. Geometry groups

- [x] 3.1 Group `PushMatrix`, the first `rtps`, `rtpt`, `stflg`, `nclip`, and `stopz` per design.md decision 3, folding the inner push into the entry that already projected. Verify the self-check: a central-half group whose backface tests all fail logs its screen point, a non-zero fail count, and a zero pass count; a group with a projection and no `rtpt` logs that it did not enter; when every projection is outside the central half, the log reports the nearest group.
- [x] 3.2 Name each reported group's outcome with the first matching rule in design.md decision 4. Verify the self-check produces, on separate groups, `black emitted primitive`, `geometry discard`, `skipped line or mask command`, `never entered its primitive pass`, and `none`. A ground primitive at `(160, 120)` with a real color leaves another group's line unchanged.

## 4. One keypress

- [x] 4.1 Arm from `HMBTN_OT_DUMP=1` and an F8 rising edge per design.md decision 1, collecting the next presented frame and then disarming. Verify the self-check drives that same arm: with the variable unset it writes no `trace object:` lines, one arm writes one frame, and the following frame writes none. `gpu_trace_frame` is not on this path.
- [x] 4.2 Document `HMBTN_OT_DUMP=1` and F8 in the README diagnostic paragraph. Verify that paragraph names the variable, the key, and the log file `build/host/hmbtn-host.log`.

## 5. Shipping bin

- [x] 5.1 Build the host, stand facing the shipping bin, run with `HMBTN_OT_DUMP=1`, press F8 once, and paste the `trace object:` block under this task. Verify the block has the walk line, the center line, the lights line, and one outcome line per reported group. Do not change what is drawn. Verify `git diff -- src/main src/overlay src/include` is empty. Verified 2026-11-10 in the running game after the `gte_rtv0` translation fix (`pc-port-rtv0-translation`): the object now draws in place and animates; the host forced-color and bed hacks are no longer needed.

## 6. Host build

- [x] 6.1 Build with the MinGW command in the README. Verify the build finishes and `HMBTN_SELFTEST=1` prints the checks from 1.1, 2.1, 2.2, 3.1, 3.2, and 4.1 as passing.

## Workflow follow-up

- Archive `pc-port-missing-objects` after task 6.1 and the pasted log in 5.1. The picture fix is a later change.
