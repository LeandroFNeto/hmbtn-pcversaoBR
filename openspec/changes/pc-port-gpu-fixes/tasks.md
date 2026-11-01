# Tasks

## 1. GTE registers

- [x] 1.1 Store the back color and the two 3 × 3 matrices in `gte.c` per design.md decision 2, and expose them on the existing settings snapshot. Verify `HMBTN_SELFTEST=1` reports `SetBackColor(10, 20, 30)` as `(10, 20, 30)` and that each matrix setter returns the nine entries it was given.
- [x] 1.2 Keep packet colors independent of those registers. Verify the self-check draws a gouraud quad whose corner color differs from the back color set in 1.1, reads one corner pixel, and matches the packet color.

## 2. Texture window

- [x] 2.1 Pack the `RECT` into the `0xE2` word in `SetTexWindow` and in `SetDrawMode`'s second word per design.md decision 3. A null rectangle still writes 0. Verify the self-check: mask 8, offset 0 in X encodes the word whose sample of U 9 is U 1, and a null `SetDrawMode` window does not emit `0xE2`.
- [x] 2.2 Apply that window in the fragment shader from static uniforms, flushing the batch when it changes, with the five mask bits done in arithmetic. Verify the same U 9 → U 1 case through a drawn sample, and that a second `DrawOTag` with no window command still uses the window. Confirm the vertex buffer size is unchanged.

## 3. Draw-buffer readback

- [x] 3.1 Record the draw-environment clip in `PutDrawEnv`. In `StoreImage`, refresh only the overlap with the back buffer per design.md decision 1, using the static row buffers, then copy the rectangle as today. Verify `HMBTN_SELFTEST=1`: an 8 × 8 block drawn in a known color inside `(0, 0, 320, 240)` stores that 15-bit color and a second store without drawing returns it; a store of `(0, 240, 320, 240)` after a frame drawn into that buffer returns the frame; a 16 × 1 CLUT row and `(896, 256, 64, 256)` are unchanged by the frame.

## 4. Scenery

- [x] 4.1 On one farm frame and one flashback frame, log the trace from design.md decision 4 (non-zero `0xE2` words, whether each black center is covered, and any line or fill sharing a packet). Verify the log names the three regions from the spec: left of the house, the bed, and the flashback background.
- [x] 4.2 Apply the single matching branch from decision 4, or stop and paste the log here if none matches. Verify a capture of each of the three views shows scenery in that region rather than a solid black rectangle, and `git diff -- src/main src/overlay src/include` is empty.

  2026-11-01: none of the three branches matches. No code was added for a fourth theory. `git diff -- src/main src/overlay src/include` is empty. Every traced frame had `e2 0` and `line 0` `fill 0`.

  Left of the house, still camera (place 1, scene 0x0):

  ```
  trace gpu: frame vblank 6480 place 1 scene 0x0 prims 524 overflow 0 black 1 e2 0 word 0x00000000 line 0 cmd 0x00 next 0x00 fill 0 next 0x00
  trace gpu: left of the house center (26,110) pixels 1040 covered 1 cmd 0x64 page 960,0 depth 8 clut 0,490 uv 0,201 word 0x0000 win 0,0,0,0
  ```

  The same center is sometimes the top primitive `cmd 0x2a` untextured `rgb 20,0,0` instead. As the camera moved, that dark quad's center walked off the screen and the blob count went to 0.

  The bed, inside (place 2, scene 0x50). The full-screen line is the fade in. The region that stays is uncovered:

  ```
  trace gpu: frame vblank 25080 place 2 scene 0x50 prims 431 overflow 0 black 1 e2 0 word 0x00000000 line 0 cmd 0x00 next 0x00 fill 0 next 0x00
  trace gpu: the bed center (254,170) pixels 16624 covered 0
  ```

  Flashback (place 54, scene 0xe7). While that scene was on screen the blob count was 0. The lines that name the region are the fade, an untextured quad over the whole frame, not a missing tile:

  ```
  trace gpu: frame vblank 17880 place 54 scene 0xe7 prims 629 overflow 0 black 0 e2 0 word 0x00000000 line 0 cmd 0x00 next 0x00 fill 0 next 0x00
  trace gpu: flashback background center (158,118) pixels 75824 covered 1 cmd 0x2a untextured rgb 107,107,107
  ```

  Branch A needs a non-zero window: none. Branch B needs a covering primitive whose sample is word 0 and whose tile is elsewhere on the page: the bed's region has no primitive, and the farm hole's other cover is an untextured color. Branch C needs a line or a fill sharing the packet: both counters stayed 0, including on the uncovered bed. The three captures were not taken, because no branch changed the picture.

## 5. Host build

- [x] 5.1 Build the host with the MinGW 32-bit command in the README. Verify the build finishes without errors and `HMBTN_SELFTEST=1` prints the checks from 1.1, 1.2, 2.1, and 3.1 as passing.

## Workflow follow-up

- Archive `pc-port-gpu-fixes` after task 5.1 and the three captures in 4.2.
