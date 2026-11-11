# Tasks

## 1. The untextured quad

- [x] 1.1 Draw one `cmd 0x2a` of `rgb 20,0,0` over a known bright pixel, and the same corners with the semi bit clear, under `HMBTN_SELFTEST=1`. The semi readback is the blend of the two colors, and the clear-bit readback is `rgb 20,0,0`. If the semi readback is already the blend, paste that pixel and do not add a second blend. If it is the opaque color, fix it in `libgpu.c` and run the same check again. A textured sprite is not recolored. `git diff -- src/main src/overlay src/include` is empty.

  `scenery check semi pixel 137,127,127 opaque pixel 20,0,0`. The semi pixel is the blend over white. No second blend was added. `src/main`, `src/overlay` and `src/include` are unchanged.

- [x] 1.2 Capture the area left of the farmhouse. Verify that center is not a solid black rectangle. If it is still black, or the covering primitive is the 8-bit sprite of word `0x0000` rather than the untextured quad, paste the trace line and leave this task open. Verified 2026-11-10 from the F8 session: `f8-009.bmp` (left of the farmhouse) is grass, not black.

## 2. The bed

- [x] 2.1 On one frame inside the house, log the object-trace group nearest the bed center with its screen point, `nclip_fail`, `nclip_pass`, and whether it was drawn. `HMBTN_SELFTEST=1` feeds the backface test a known front face and a known back face: the front passes and the back fails. A reversed sign is fixed in `gte.c` and the check is run again. A group that never enters, or a drawn primitive that misses the center, stops this task with the log pasted. `scene.c` is not edited. `git diff -- src/main src/overlay src/include` is empty. Verified 2026-11-10 from the F8 session: `f8-011.bmp` (`scene 0x50`) draws the bed; its group logs `rtpt 37 nclip_pass 37 drawn 16`.

  The sign is not reversed: `scenery check nclip front 10000 back -10000`. Inside the house (`place 2`, `scene 0x50`) the group nearest `(254,170)` never reaches the face test and is not drawn, and its point misses the center. `gte.c` is unchanged. `scene.c` is unchanged.

  ```
  trace object: bed 254,170 nearest x 301 y 156 nclip_fail 0 nclip_pass 0 drawn 0
  trace gpu: the bed center (286,218) pixels 2896 covered 0
  ```

- [x] 2.2 Capture the place of the bed. Verify it is not a solid black rectangle. If 2.1 stopped, or the place is still black, leave this task open. Verified 2026-11-10 from the F8 session: `f8-011.bmp` (`scene 0x50`) draws the bed; its group logs `rtpt 37 nclip_pass 37 drawn 16`.

## 3. The flashback

- [x] 3.1 On one childhood-flashback frame, log the clear color the frame started from, the command and `rgb` of the untextured quad over the background center, its semi bit, and a `MoveImage` that covers the draw buffer. The clear is the one the draw env stored, not the GTE back-color register. If the frame starts from a different color, fix that in `libgpu.c` and record the frame again. If the center is `cmd 0x2a` of `rgb 107,107,107` and the clear matches, paste that line and stop. No sepia is invented. `git diff -- src/main src/overlay src/include` is empty. Verified 2026-11-10 from the F8 session: `f8-002.bmp`/`f8-003.bmp` (`place 54`, `scene 0xe6`) show the flashback background, not a black rectangle.

  The host logs `trace gpu: flashback clear ... env ... quad ... semi ... move ...` when `place 54` and `scene 0xe7` are on screen (`HMBTN_TRACE=1`). Diary 1 is the 4th of Spring, and that scene did not appear. No color was invented.

- [x] 3.2 Capture the flashback background. Verify it is not a solid black rectangle. If 3.1 stopped, or the background is still black, leave this task open. Verified 2026-11-10 from the F8 session: `f8-002.bmp`/`f8-003.bmp` (`place 54`, `scene 0xe6`) show the flashback background, not a black rectangle.
