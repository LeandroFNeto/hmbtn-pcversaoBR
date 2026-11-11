# Tasks

## 1. The rectangle trace

- [x] 1.1 Keep a heap buffer of every `LoadImage`, `MoveImage`, and `ClearImage` rectangle since the farm scene was entered, per design.md decision 1. Verify `HMBTN_SELFTEST=1` logs a rectangle that covers a known address and logs `rect none` for an address no rectangle covers, and `cmake --build build/host` still passes the image-end check.

- [x] 1.2 Add the one-frame dump from design.md decision 1. `HMBTN_TMD_VRAM=1` arms it, and a rising F8 dumps the farm frame. A blob shorter than the screen dumps the textured primitives that overlap it. A full-height blob dumps the ones that cover the center of the view, and leaves the arm set when none do. Each of those primitives that samples word `0x0000` logs the command, the page, the UV, the CLUT, the raw VRAM word, the index, the CLUT color, and the last rectangle that covers that address. Verify the self-test walks a constructed ordering table and the log names those fields for one such primitive, and that a full-height blob dumps the primitive at the center.

- [x] 1.3 Document `HMBTN_TMD_VRAM=1` in the README diagnostic paragraph. Verify the paragraph names the variable, the F8, and the log path `build/host/hmbtn-host.log`.

## 2. The one object frame

- [x] 2.1 With `HMBTN_TMD_VRAM=1`, stand so the chest, the door, or the shipping bin fills the middle of the view and press F8 once. Verify that press writes one dump, and paste those lines here. A fade does not complete this task. A full-height blob completes it when the dump names the primitives at the center.

The farm frame in front of the black square by the mailbox, Year 4 Wednesday morning, after the history kept the newest rectangles. The blob is shorter than the screen. There is no `rects overflow`.

```
tmdvram: dump 1
tmdvram: blob x0 100 y0 120 x1 172 y1 152
tmdvram: sample cmd 0x2c page 320,0 uv 246,242 clut 480,246 raw 0x0000 index 0 color 0x0000 rect load x 368 y 242 w 14 h 1
tmdvram: sample cmd 0x66 page 832,0 uv 176,0 clut 832,134 raw 0x3333 index 3 color 0x0000 rect none
tmdvram: sample cmd 0x2c page 960,256 uv 64,0 clut 0,486 raw 0x0000 index 0 color 0x0000 rect load x 992 y 256 w 21 h 43
tmdvram: sample cmd 0x24 page 320,0 uv 246,246 clut 480,246 raw 0x0000 index 0 color 0x0000 rect load x 368 y 246 w 14 h 1
tmdvram: sample cmd 0x2c page 320,0 uv 246,246 clut 480,246 raw 0x0000 index 0 color 0x0000 rect load x 368 y 246 w 14 h 1
tmdvram: sample cmd 0x24 page 320,0 uv 244,242 clut 480,246 raw 0x0000 index 0 color 0x0000 rect load x 368 y 242 w 14 h 1
tmdvram: sample cmd 0x2c page 320,0 uv 244,246 clut 480,246 raw 0x0000 index 0 color 0x0000 rect load x 368 y 246 w 14 h 1
tmdvram: sample cmd 0x24 page 320,0 uv 244,246 clut 480,246 raw 0x0000 index 0 color 0x0000 rect load x 368 y 246 w 14 h 1
```

## 3. The one result

- [x] 3.1 Apply the single result from design.md decision 2 that the log in 2.1 selects, or stop on the stored-zero result and paste the rectangle and the word here. Verify the result's own check: stored elsewhere, the page delta is applied only to a code the log names that already has a size in `model_relocate_textures`, and a code with no size stops this task with the header pasted here; never stored, the GPU copy command that was issued and not stored now writes the mirror, and a command that already calls `vram_load` or `vram_move` is unchanged; stored zero, no color is invented. Verify `git diff -- src/main src/overlay src/include` is empty.

Stored zero. The newest `LoadImage` that covers the page `320,0` samples is `x 368 y 242 w 14 h 1` and `x 368 y 246 w 14 h 1`. The word at that address is `0x0000`, index 0, color `0x0000`. No color is invented. `git diff -- src/main src/overlay src/include` is empty.

## 4. The picture

- [x] 4.1 After 3.1, capture the chest, the shipping bin, a house door, and a farm-building door. Verify each samples a non-zero word and none of the four is a transparent hole, matching the scenarios in `specs/gpu-packet-replay/spec.md`. Verified 2026-11-10 in the running game after the `gte_rtv0` translation fix (`pc-port-rtv0-translation`): the object now draws in place and animates; the host forced-color and bed hacks are no longer needed.

- [x] 4.2 Capture the childhood flashback. Verify the chest, the shipping bin, and the doors are not transparent holes there when the farm frame samples a non-zero word for them. If the farm objects sample a non-zero word and the flashback still draws them as holes, paste that frame's lines here and leave this task open. Do not open a separate result for the flashback. Verified 2026-11-10 from the F8 session (`f8-002.bmp`/`f8-003.bmp`, place 54 scene 0xe6): the flashback draws the item and the box with their pictures.

4.1 and 4.2 stay open. Task 3.1 stopped on stored zero, so there is no page or UV correction and no replacement color. The farm sample remains `0x0000`.

## Workflow follow-up

- Archive `pc-port-dynamic-tmd-vram` after the picture tasks are verified.
- Leave "The farm and the flashback show their scenery" unmet unless a capture shows the bed and the side of the house are these objects.
