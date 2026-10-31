# Proposal

## Why

The decompiled game builds only as a matching PlayStation executable: CMake always loads the PS1 toolchain (GCC 2.95.2 and PSY-Q 4.7), and the sources call Sony libraries (`libgpu`, `libgte`, `libgs`, and others) that do not exist on a PC. A native Windows/Linux port needs a host build that compiles those same sources before any window, input, or renderer exists.

## What Changes

- Add a host CMake configuration, selected explicitly, that compiles the current C sources with a modern GCC or Clang on Windows and Linux and links one executable.
- Keep the existing PS1 matching build as the default. Its toolchain, SHA-1 check, and disc targets stay unchanged.
- Add host stubs, under `src/pc_port/`, for every PSY-Q header the sources include today, not only `libgpu.h`, `libgte.h`, and `libetc.h`. The tree also includes `libgs.h`, `libcd.h`, `libapi.h`, `libpad.h`, `libsnd.h`, `libspu.h`, `libmcrd.h`, `libmcx.h`, `libmath.h`, `kernel.h`, `abs.h`, `memory.h`, `inline_c.h`, and `gtemac.h`. Stub functions link and return without talking to hardware.
- Leave gameplay sources in `src/main/` and `src/overlay/` unchanged. The two files that include the project's `gte.h` (`model.c`, `vec_length.c`) emit MIPS GTE opcodes; the host build must compile them without editing those files.
- Keep the host entry in `src/pc_port/src/main_pc.c`. The game's `main` compiles as `ps1_main` through the existing `-Dmain=ps1_main` definition and is linked but not called.
- Do not translate pads, call `ps1_main`, or read a `/mods` folder. Those are later changes.

Assumption: this change owes a successful configure, compile, and link of the game sources. Running `ps1_main` is not a success criterion; it still stores through PS1 addresses, which `pc-port-ps1-memory` maps.

## Capabilities

### New Capabilities

- `pc-host-build`: A CMake host target builds the decompiled sources with a modern compiler into one linked executable on Windows and Linux, beside the untouched PS1 matching build.
- `psyq-api-stubs`: Host headers and linkable definitions supply the PSY-Q types and symbols the current sources reference, with no hardware behavior.

### Modified Capabilities

- `host-gl-present`: Removes "Game sources stay out of this link", because this change puts them on the link.

## Impact

- New tree `src/pc_port/` (headers plus stub translation units) and a host CMake file. The root `CMakeLists.txt` gains a branch that skips `cmake/psx-toolchain.cmake` and the SHA-1 `check` target.
- No third-party dependency beyond the SDL2 package and system OpenGL the host window already links. The target stays 32-bit (i686).
- The PS1 include path (`PSYQ_INCLUDE_DIR`) is not used by the host target. Angle-bracket SDK includes resolve to the stubs.
- `src/include/gte.h` stays the PS1 header. The host compile of `model.c` and `vec_length.c` needs an include-path or flag solution that does not rewrite those files.
- Out of scope: pad mapping, running `ps1_main`, the PS1 address space, `CdRead` redirection, PNG/`/mods` loading, and any gameplay refactor.
