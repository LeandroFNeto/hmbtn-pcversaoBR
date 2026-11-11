# Tasks

## 1. One armed frame

- [x] 1.1 While `HMBTN_DUMP_TEXTURES=1`, one rising F8 writes each textured primitive of that frame as an uncompressed BMP in `mods/textures/dump/` beside the executable, named `pageX_pageY_u0_v0_u1_v1.bmp`. Holding F8 writes nothing on the next frame, and the variable unset writes nothing. A pixel is the stored color of that texel, CLUT included, and a stored `0x0000` stays black. Two primitives that share a page and UV bounds write one file. `HMBTN_SELFTEST=1` builds one non-zero CLUT color and one stored zero, reads both BMP pixels back, and drives the edge (one write, a held F8 writes nothing, an unarmed F8 writes nothing). The log lines go in `build/host/hmbtn-host.log`. The README diagnostic paragraph names `HMBTN_DUMP_TEXTURES`, F8, and `mods/textures/dump/`.

## 2. The dump stays out of the frame

- [x] 2.1 The write does not change the VRAM word of the texel it read, and a BMP that exists only under `dump/` is not a replacement: `texture_resolve` for that page and UV still returns 0. `HMBTN_SELFTEST=1` checks both, and `git diff -- src/main src/overlay src/include` is empty.
