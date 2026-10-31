# Tasks

## 1. Host build switch

- [x] 1.1 Create `src/pc_port/include/`, `src/pc_port/include/sys/`, `src/pc_port/src/`, and `cmake/host.cmake` with executable `hmbtn-host` and one empty stub translation unit. Verify `cmake -S . -B build-host -DHMBTN_HOST=ON` configures without loading `cmake/psx-toolchain.cmake`.
- [x] 1.2 Add `option(HMBTN_HOST OFF)` at the top of the root `CMakeLists.txt` and return into `cmake/host.cmake` before the toolchain assignment. Verify a configure without `-DHMBTN_HOST=ON` still selects `cmake/psx-toolchain.cmake` (configure trace or the toolchain error from a machine without the PS1 tools).
- [x] 1.3 Add every `psx_exe` and `psx_overlay` `SOURCES` file to `hmbtn-host`, with `src/pc_port/include` before the game include directories, `CMAKE_C_EXTENSIONS` on, and `-funsigned-char`. Verify the generated target lists `src/main/main.c` and `src/overlay/11_GF_FARM/farm_main.c`, still lists `main_pc.c`, and the link libraries are SDL2 and `opengl32` only.

## 2. Integer types and libgpu

- [x] 2.1 Add `src/pc_port/include/sys/types.h` typedefs for `u_char`, `u_short`, `u_long`, and `u_int` as 8/16/32/32-bit types. Verify a host compile of a throwaway file that includes it reports `sizeof(u_long) == 4`.
- [x] 2.2 Add `src/pc_port/include/libgpu.h` and `src/pc_port/src/libgpu.c` with the `RECT`, `TILE`, `SPRT`, `DR_MODE`, `POLY_FT4`, `DRAWENV`, and `DISPENV` shape and the functions in design.md. Verify each function returns without opening a device, and that compiling `src/main/graphics/prim.c` for the host no longer reports a missing `libgpu.h` type named in `prim.h`.

## 3. MIPS header shims

- [x] 3.1 Keep `src/pc_port/include/gte.h` (from `pc-port-gl-renderer`, `gte_*` macros as host C calls, no `__asm__`). Verify the host compile of `src/main/geometry/model.c` and `src/main/geometry/vec_length.c` does not fail on a MIPS `.word` operand.
- [x] 3.2 Add `src/pc_port/include/cpu_asm.h` with the same macro names as `src/include/cpu_asm.h` and empty statement bodies. Verify the host compile of `system.c`, `anim.c`, `farm_frame.c`, and `farm_main.c` does not fail on a MIPS register operand.

## 4. Remaining PSY-Q surface

- [x] 4.1 Add host headers for `libgte.h`, `libgs.h`, `libetc.h`, `libcd.h`, `libapi.h`, `libpad.h`, `libsnd.h`, `libspu.h`, `libmcrd.h`, `libmcx.h`, `libmath.h`, `kernel.h`, `abs.h`, `memory.h`, `inline_c.h`, and `gtemac.h`, declaring only types and prototypes the current sources use. Verify a host compile no longer stops on a missing header from that list.
- [x] 4.2 Add one stub `.c` per library whose symbols are still undefined, each returning zero, NULL, or the caller's pointer. Verify `cmake --build build/host` links `hmbtn-host` with no undefined PSY-Q symbol and no PSY-Q library on the link line, and that the existing window still opens, shows the green quad, and closes with exit code 0.

## 5. Integration

- [x] 5.1 Rebuild `hmbtn-host` on Windows with GCC or Clang. Verify the executable file exists and `git diff -- src/main src/overlay src/include` is empty.
- [ ] 5.2 Run the same `cmake -S . -B build-host -DHMBTN_HOST=ON` and build on Linux with GCC or Clang. Verify `hmbtn-host` links there with the same flags. If this session has no Linux compiler, leave this task open and record the Windows compiler and flags the Linux run must reuse.

  Open: the WSL Ubuntu here has no `gcc`, and installing it needs interactive `sudo`. The Windows build that linked used MinGW.org GCC 6.3.0 (`mingw32`, i686), `-G "MinGW Makefiles"`, `-DSDL2_DIR=C:/SDL2/SDL2-2.32.10/i686-w64-mingw32/lib/cmake/SDL2`, and target flags `-funsigned-char -fcommon` with GNU C extensions. A Linux run needs `gcc-multilib`, 32-bit SDL2 and GL development packages, `-m32`, and three `cmake/host.cmake` adjustments that are Windows-only today: link the system GL instead of `opengl32`, drop `--image-base`, and pass `--defsym=overlay_low=...` without the leading underscore.

## Workflow follow-up

- Archive the change after review, once `/opsx-apply` has finished and the tasks above are checked.
- After archiving, edit the Purpose of `openspec/specs/host-gl-present/spec.md` to drop "without taking the game sources into the host link".
