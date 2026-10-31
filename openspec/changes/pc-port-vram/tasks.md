# Tasks

## 1. The VRAM and its transfers

- [x] 1.1 Add `src/pc_port/include/vram.h` and `src/pc_port/src/vram.c` with the static 1024 × 512 array, the dirty row range, and `LoadImage`, `StoreImage`, `MoveImage`, `ClearImage`, `ClearImage2` per design.md section 2, removing their stubs from `libgpu_stub.c`. Verify `HMBTN_SELFTEST=1` checks: a 16 × 2 load at (640, 0) stores back equal; a load at x = 1020 with width 8 wraps to x = 0; an overlapping 8 × 8 move from (512, 0) to (516, 0) keeps the source's original words; a clear of (255, 0, 0) writes `0x001F`.
- [x] 1.2 Add `vram_observe_write` (default no-op) and call it from every transfer. Verify a check that installs a counting observer sees one call per `LoadImage`, `ClearImage`, and `MoveImage`.
- [x] 1.3 Complete `SetDrawLoad` and `SetDrawMove` to write the SDK's command words. Verify a check that a `DR_LOAD` built with `SetDrawLoad` for a 16 × 1 rectangle has command `0xA0`, the rectangle words, and length 3 + 8.

## 2. VRAM in OpenGL

- [x] 2.1 Create the `1024 × 512` `GL_LUMINANCE_ALPHA` texture in `gpu_gl_init`, upload the dirty rows before each batch, and add `gpu_gl_shutdown` that deletes the texture, program, and buffer, called from `main_pc.c` on both exit paths. Verify the build links, the window opens and closes with exit code 0, and the shutdown runs on a close during `VSync` (log line).
- [x] 2.2 Replace the vertex format and shader per design.md section 4 (UV, page, depth, CLUT, flags; 4-bit, 8-bit, 15-bit decode; `0x0000` discarded; modulation unless raw). Verify off-screen checks with known VRAM contents, read back with `glReadPixels` before any swap: a raw 4-bit `SPRT` whose index 1 maps to red draws red; an 8-bit sprite draws its CLUT color; a 15-bit sprite draws its word's color; a `0x0000` texel leaves the cleared color; color (64, 64, 64) halves a white texel.

## 3. The walker

- [x] 3.1 Decode all polygon (`0x20`–`0x3F`) and rectangle (`0x60`–`0x7F`) commands with the vertex strides in design.md section 3, keeping lines skipped. Verify checks that a `POLY_G3`, a `POLY_FT3`, a `POLY_GT4`, a `TILE_16`, a `TILE_1`, and an `SPRT_8` each draw at their positions with the expected colors.
- [x] 3.2 Track `GpuState` from `0xE1`–`0xE6` words (every word of `DR_MODE`), textured polygons' page words, and `PutDrawEnv`; run `DR_LOAD` and `DR_MOVE` in chain order. Verify a chain `DR_TPAGE` (page 640, 4-bit) → `SPRT` samples that page, and a chain `DR_LOAD` (new CLUT row) → `SPRT` draws the new colors.
- [x] 3.3 Add semi-transparency per the blend table in design.md section 4, cutting batches around mode 2. Verify a mode-0 white `TILE` over black reads about 50% gray, a mode-1 gray over gray adds, a mode-2 black `POLY_F4` leaves the frame unchanged, and a textured mode-0 sprite blends only its texels with bit 15 set.
- [x] 3.4 Make `ClearImage` over the displayed area and `PutDrawEnv` with `isbg` set the next frame's clear color. Verify a check that clearing (0, 0, 320, 480) to (0, 0, 255) makes the next frame's untouched pixels blue.

## 4. Seam, memory, and the title

- [x] 4.1 Add `texture_resolve` returning `TEXTURE_FROM_VRAM` and call it once per textured primitive when batching. Verify a check that a counting resolver sees one call per textured primitive in a chain and none for untextured ones.
- [x] 4.2 With `HMBTN_RUN_GAME=ON` and the extracted disc, run 10 seconds and capture the window with `build/host/capture.ps1`. Verify the captured frame is not flat `(128,128,128)`, has at least 32 distinct colors, and closing exits with code 0. Embed the capture in the change summary. If the frame is still flat or wrong, record which packets the title drew (codes and counts from the exit log) here and pause.
- [x] 4.3 Run the game for one minute and compare the process's private bytes at 10 s and at 60 s. Verify they differ by less than 1 MB, and `git diff -- src/main src/overlay src/include` is empty.

## Workflow follow-up

- Archive `pc-port-vram` after review, once these tasks are checked.
- A `/mods` change implements `vram_observe_write` and `texture_resolve` per design.md section 5.
