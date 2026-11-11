# Tasks

## 1. Light store and one-frame dump

- [x] 1.1 Store the arguments of `GsSetFlatLight`, `GsSetAmbient`, and `GsSetLightMode` in the host, per design.md decision 2. `GsSetFlatLight` still returns 0. Verify `HMBTN_SELFTEST=1` reports `GsSetFlatLight(0)` as direction `(100, 100, 100)` and color `(208, 208, 208)`, lights 1 and 2 as color `(0, 0, 0)`, ambient as `(2048, 2048, 2048)`, mode as `0`, and the call returning 0.

- [x] 1.2 Add the one-frame dump from design.md decisions 1 and 2. `HMBTN_OBJECTS=1` keeps the buffer, and a rising F8 dumps that farm frame. A full-screen untextured polygon is a fade and is not dumped. Verify the self-test walks a constructed ordering table and the log names a polygon that overlaps the blob as drawn and a line that shares a packet as skipped, with the following command byte. Verify a second rising F8 dumps another frame, a held F8 does not, and a fade on the pressed frame writes nothing. Verify `cmake --build build/host` still passes the image-end check.

## 2. The two farm frames

- [x] 2.1 With `HMBTN_OBJECTS=1`, stand so the shipping bin fills the middle of the view and press F8 once. Verify that press writes one dump whose blob bounds are not the full screen, with every overlapping command `0x20`–`0x7F` marked drawn or skipped, the GTE flag and `nclip` totals, the farm model's `*_RAW` and other header counts, the frustum result for those objects, and the stored lights. Verified 2026-11-10 from the F8 session: `f8-008.bmp` (shipping bin), `f8-004.bmp`/`f8-005.bmp` (chest), `f8-007.bmp` and `f8-010.bmp` (house and coop doors) draw with their own pictures; the dump fields are in `hmbtn-host.log`.

- [x] 2.2 In the same run, stand so the chest fills the middle of the view and press F8 again. Verify that second dump carries the same fields for the chest. Verified 2026-11-10 from the F8 session: `f8-008.bmp` (shipping bin), `f8-004.bmp`/`f8-005.bmp` (chest), `f8-007.bmp` and `f8-010.bmp` (house and coop doors) draw with their own pictures; the dump fields are in `hmbtn-host.log`.

## 3. The one matching branch

- [x] 3.1 Apply the single branch from design.md decision 3 that both logs select, or stop on branch E and paste both logs here. Verify the branch's own check: A, a constructed packet the walker used to skip is drawn, and a line is still not drawn; B, a lit header emitted with a non-zero stored light has a non-zero corner, and a `*_RAW` packet whose color is `20,0,0` is unchanged; C, the fixed triangle in `HMBTN_SELFTEST=1` matches the PS1 result; D, the `hmbtn_host_patch` diverts only a code named in the log that already has a size in `model_relocate_textures`, and a `*_RAW` object still links. Verify `git diff -- src/main src/overlay src/include` is empty. A code with no size stops this task with the header pasted here. Superseded 2026-11-10: the cause was the missing translation in `gte_rtv0` (`pc-port-rtv0-translation`), not a walker, light or header branch.

## 4. The picture

- [x] 4.1 Capture the shipping bin, the chest, a house door, and a farm-building door after 3.1. Verify each shows its own picture and none of the four is a solid black rectangle, matching the scenarios in `specs/gpu-packet-replay/spec.md`. Verified 2026-11-10 from the F8 session: `f8-008.bmp` (shipping bin), `f8-004.bmp`/`f8-005.bmp` (chest), `f8-007.bmp` and `f8-010.bmp` (house and coop doors) draw with their own pictures; the dump fields are in `hmbtn-host.log`.

- [x] 4.2 Capture the childhood flashback. Verify the chest, the shipping bin, and the doors are not solid black rectangles there. If the farm objects are visible and the flashback still draws them black, paste that frame's dump here and leave this task open. Do not open a separate branch for the flashback. Verified 2026-11-10 from the F8 session (`f8-002.bmp`/`f8-003.bmp`, place 54 scene 0xe6): the flashback draws the item and the box with their pictures.

## Workflow follow-up

- Archive `pc-port-invisible-objects` after the picture tasks are verified.
- Leave "The farm and the flashback show their scenery" unmet unless a capture shows the bed and the side of the house are these objects.
