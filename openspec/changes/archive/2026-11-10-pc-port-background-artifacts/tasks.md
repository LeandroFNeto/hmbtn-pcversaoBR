# Tasks

## 1. Blend rate 2

- [x] 1.1 Under `HMBTN_SELFTEST=1`, draw a semi-transparent untextured quad of `rgb 255,255,255` at blend rate 2 over `rgb 255,255,255`, and the same quad over `rgb 40,0,0`. Both readbacks are `rgb 0,0,0`. If either readback is `rgb 255,255,255` or the destination left unchanged, correct the equation in `libgpu.c` and run the same pair again. If both are already `rgb 0,0,0`, leave the equation as it is. The existing rate-0 check still reads about `137,127,127` for `rgb 20,0,0` over white, and the black-source rate-2 check in `vram_check.c` is not this proof. No replacement color is written. `git diff --stat -- src/main src/overlay src/include` is empty.

  `bgtrace check rate2 white-over-white 0,0,0 white-over-40 0,0,0`. The equation was left as it is. The rate-0 pixel stayed `137,127,127`. `src/main`, `src/overlay` and `src/include` are unchanged.

## 2. The background record

- [x] 2.1 From `cmake/host.cmake`, wrap `ClearImage` and `ClearImage2`. The wrappers live in `libgpu.c`, log only while the record is armed, and call the real functions. An armed frame stores each primitive's semi bit and blend rate, lists an untextured quad of at least 160 by 120, records every clear with its rectangle and whether it overlaps the display, records `0xE3`, `0xE4`, `0xE5`, `0xE6`, and `0x02` without executing them, and reads the color write mask and the stencil write mask without setting them. The self-check forces a house scene on one constructed frame: two primitives cover `(160,120)` and the later one is named, with its command, raw color, semi bit, and rate; `(40,40)` is none; a quad smaller than 160 by 120 is not in the large-quad list; the clear rectangle is named; a window clear after the center primitive says `clear after`; the mask word is named and the color write mask is unchanged. A frame that is neither a house scene nor `place 54` scene `0xe6` or `0xe7` writes one skip line and no edge block. With the flag off, a clear is not logged. `git diff --stat -- src/main src/overlay src/include` is empty.

- [x] 2.2 With `HMBTN_BG_TRACE=1`, one rising F8 on a house scene (`0x50`–`0x5B`) and one on the flashback (`place 54`, scene `0xe6` or `0xe7`) each write the five edge lines, the large-quad list, the clears, and the mask line to `build/host/hmbtn-host.log`. The `f8-NNN.bmp` files stay the existing capture. Paste those lines. If the white quad's rate is 2 and 1.1 already read `rgb 0,0,0`, do not recolor it. If a point says `clear after`, do not narrow the clear in this task. A scene that does not appear leaves this task open. `pc-port-scenery-fixes` is not edited.

  `f8-001.bmp`, `place 54`, `scene 0xe6`. The center is `cmd 0x2a rgb 20,0,0 semi 1 abr 2`, pixel `13,25,16`. No point says `clear after`. The rate-2 equation stays.

  ```
  bgtrace: place 54 scene 0xe6 point 40,40 cmd 0x2a rgb 20,0,0 semi 1 abr 2 pixel 173,132,61
  bgtrace: place 54 scene 0xe6 point 280,40 cmd 0x2a rgb 20,0,0 semi 1 abr 2 pixel 156,117,57
  bgtrace: place 54 scene 0xe6 point 40,200 cmd 0x38 rgb 244,223,189 semi 0 abr 2 pixel 247,213,162
  bgtrace: place 54 scene 0xe6 point 280,200 cmd 0x38 rgb 244,223,189 semi 0 abr 2 pixel 247,213,162
  bgtrace: place 54 scene 0xe6 point 160,120 cmd 0x2a rgb 20,0,0 semi 1 abr 2 pixel 13,25,16
  bgtrace: quad cmd 0x2a 0,0 320,240 rgb 20,0,0 semi 1 abr 2
  bgtrace: quad cmd 0x38 28,166 291,224 rgb 244,223,189 semi 0 abr 2
  bgtrace: mask color 1,1,1,1 stencil 4294967295 e6 0 word 0x00000000
  bgtrace: draw e3 0xe3000000 e4 0xe403bd3f e5 0x00000000 fill 0 0,0 0x0 rgb 0,0,0
  ```

  `f8-002.bmp`, `place 2`, `scene 0x50`. No large untextured quad and no clear on this frame. The center stays `cmd 0x2c`.

  ```
  bgtrace: place 2 scene 0x50 point 40,40 cmd 0x3c rgb 84,84,84 semi 0 abr 0 pixel 94,68,27
  bgtrace: place 2 scene 0x50 point 280,40 cmd 0x64 rgb 128,128,128 semi 0 abr 0 pixel 248,210,152
  bgtrace: place 2 scene 0x50 point 40,200 cmd 0x3c rgb 128,128,128 semi 0 abr 0 pixel 105,85,55
  bgtrace: place 2 scene 0x50 point 280,200 cmd 0x3c rgb 84,84,84 semi 0 abr 0 pixel 56,43,27
  bgtrace: place 2 scene 0x50 point 160,120 cmd 0x2c rgb 128,128,128 semi 0 abr 0 pixel 41,41,41
  bgtrace: mask color 1,1,1,1 stencil 4294967295 e6 0 word 0x00000000
  bgtrace: draw e3 0x00000000 e4 0x00000000 e5 0x00000000 fill 0 0,0 0x0 rgb 0,0,0
  ```

- [x] 2.3 After 1.1 and 2.1 pass, the README diagnostic states the measured rate-2 readback, that `HMBTN_BG_TRACE=1` and one F8 on the house or the flashback write `bgtrace:` with the edge command, raw color, semi bit, blend rate, and whether a clear came after, and that the pixel is not replaced. The page-and-UV sentence and the `tmd_` sentence stay. The black house and the black flashback stay under "Ainda falta".

## Workflow follow-up

- The scenery requirement that those regions not be a solid black rectangle stays open. This change records the math. It does not paint the house or the flashback.
- Do not apply draw-area commands, a mask, or VRAM fill command `0x02` from the log alone. A later change can, after 2.2 names which one covers the black point.
