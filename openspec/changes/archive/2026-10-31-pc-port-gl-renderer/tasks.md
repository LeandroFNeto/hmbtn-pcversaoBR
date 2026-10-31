# Tasks

## 1. OpenGL on the existing window

- [x] 1.1 In `cmake/host.cmake`, link `opengl32` on Windows and keep `find_package(SDL2 REQUIRED)`. In `main_pc.c`, create the window with `SDL_WINDOW_OPENGL`, request a 2.1 context, clear, and swap each iteration. Verify `cmake -S . -B build/host -DHMBTN_HOST=ON -G "MinGW Makefiles" -DCMAKE_C_COMPILER=C:/MinGW/bin/gcc.exe -DSDL2_DIR=C:/SDL2/SDL2-2.32.10/i686-w64-mingw32/lib/cmake/SDL2` and `cmake --build build/host` succeed, and that closing the window ends `hmbtn-host.exe`.
- [x] 1.2 Confirm the host link line contains `main_pc.c` and `libgpu.c` and no file from `src/main/` or `src/overlay/`. Verify by reading `build/host/CMakeFiles/hmbtn-host.dir/link.txt` or the build.make source list.

## 2. Ordering-table replay

- [x] 2.1 Declare `DrawOTag` in `src/pc_port/include/libgpu.h` and implement the walker in `src/pc_port/src/libgpu.c` for command codes `0x28`, `0x2C`, `0x3C`, and `0x64`, skipping every other code. Verify `cmake --build build/host` compiles `libgpu.c` with no errors.
- [x] 2.2 Add a static `POLY_F4` and call `DrawOTag` on it from the host loop, using a color that is not the clear color. Verify a rebuild succeeds and the running window shows that color, then still exits when closed.

## 3. Host GTE

- [x] 3.1 Add `src/pc_port/src/gte.c` and `src/pc_port/include/gte.h` with the matrix, translation, offset, and screen-distance projection from the design, and no `__asm__`. Verify the host link succeeds and a startup check of the identity vertex `(0,0,z)` at offset `(160,120)` returns success before the window loop.
- [x] 3.2 Extend that check to a vertex with positive X so screen X is greater than 160 and screen Y stays 120, and to a vertex with depth less than or equal to zero so the front-of-camera check fails. Verify the process opens the window only when both checks succeed.

## 4. Integration

- [x] 4.1 Rebuild `hmbtn-host` with the same MinGW and `SDL2_DIR` command. Verify the executable exists, the window shows the flat quad, and `git diff -- src/main src/overlay src/include` is empty.

## Workflow follow-up

- Archive `pc-port-gl-renderer` after review, once these tasks are checked and the quad is visible.
