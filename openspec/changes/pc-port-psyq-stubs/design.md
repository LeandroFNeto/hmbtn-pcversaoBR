# Design

## Context

See proposal.md for why this change exists. The facts that shape the approach:

- `CMakeLists.txt` sets `cmake/psx-toolchain.cmake` before `project()` whenever `CMAKE_TOOLCHAIN_FILE` is empty, then builds fourteen PS1 images and a SHA-1 `check` target. A host compiler never enters that path.
- Game and overlay sources include PSY-Q headers with angle brackets (`libgpu.h`, `libgte.h`, `libgs.h`, `libetc.h`, `libcd.h`, `libapi.h`, `libpad.h`, `libsnd.h`, `libspu.h`, `libmcrd.h`, `libmcx.h`, `libmath.h`, `kernel.h`, `abs.h`, `memory.h`, `inline_c.h`, `gtemac.h`). `prim.h` uses `POLY_FT4`, `SPRT`, `DR_MODE`, `TILE`, and `u_long`. `main()` writes `DRAWENV.ofs` and `DISPENV.screen`.
- `src/include/gte.h` and `src/include/cpu_asm.h` are quoted includes. They expand to MIPS `__asm__`. `gte.h` is included by `model.c` and `vec_length.c`. `cpu_asm.h` is included by `system.c`, `model.c`, `anim.c`, `farm_frame.c`, and `farm_main.c`. Quoted lookup starts in the including file's directory, then the `-I` list, so an earlier `-I` can shadow both headers without editing those files.
- A scan of the 113 `.c` files found no duplicate global function names, so one host link of every translation unit is plausible. Link order, which the PS1 images depend on, does not matter for this stub link.

## Goals / Non-Goals

**Goals:**

- One CMake switch, default off, that configures a host executable with GCC or Clang and never loads the PS1 toolchain.
- Stub headers and definitions only for symbols and types the current sources use, grown from the compiler and linker, not a copy of the PSY-Q 4.7 headers.
- Host replacements for `gte.h` and `cpu_asm.h` that keep the same macro names and emit no MIPS assembly.

**Non-Goals:**

- Calling `ps1_main`, pad mapping, the PS1 address space, or CD/mod I/O. See the proposal's out-of-scope list. The SDL window, OpenGL presentation, `DrawOTag`, and host GTE from `pc-port-gl-renderer` stay as they are.
- Matching PS1 struct padding beyond what the sources name. `u_long` is still fixed at 32 bits so later phases do not retype the ordering table.

## Decisions

### 1. Host build is an early return in the root list

`HMBTN_HOST` is an option read before `project()`. When it is ON, the root list calls `project(hmbtn-host C)`, includes `cmake/host.cmake`, and returns. When it is OFF, the file is unchanged from the toolchain assignment downward.

Alternative considered: a second top-level `CMakeLists` for the host. Rejected because the sources and include directories already live in this tree, and a second entry point would drift from the `SOURCES` lists.

### 2. Port code lives in `src/pc_port/`

Create these directories first, and nothing under a renderer or window folder:

```
src/pc_port/include/          angle-bracket stubs and the two shadowed headers
src/pc_port/include/sys/      host sys/types.h
src/pc_port/src/              one .c per stubbed library
cmake/host.cmake              host target only
```

`cmake/host.cmake` puts `src/pc_port/include` ahead of the existing game include directories. That resolves `<libgpu.h>` and also `"gte.h"` / `"cpu_asm.h"`. It is a normal `-I` directory, not `-isystem`, so it wins over the host's own `<sys/types.h>`.

The only edit outside that tree and `cmake/host.cmake` is the branch at the top of the root `CMakeLists.txt`.

### 3. Compile every current `.c` into one executable

`cmake/host.cmake` owns an executable `hmbtn-host` whose sources are `main_pc.c`, the existing stubs, the `SOURCES` of `psx_exe`, and every `psx_overlay`. Flags: `CMAKE_C_EXTENSIONS` on (the sources are GNU C), `-funsigned-char` (the PS1 build passes `-D__CHAR_UNSIGNED__`), and no `-Werror`. The existing `-Dmain=ps1_main` definition renames the game entry, so `main_pc.c` stays the process entry. The SDL2 and `opengl32` links stay; no Python requirement.

The target is i686. Game code casts pointers to `u32` and stores them in packet words, so a 64-bit build would truncate them. MinGW.org GCC 6.3.0 is already 32-bit; a 64-bit toolchain must pass `-m32`.

The host defines its own `psx_exe()` and `psx_overlay()` that only collect `SOURCES`, then adds the same `src/` subdirectories, so the host and PS1 source lists cannot drift.

One file needs a construct GCC 2.95 accepted and modern GCC rejects with no relaxing flag: `text.c` defines `text_fmt_dec`, `text_fmt_hex`, and `text_fmt_int` `static` after `text.h` declares them `extern`. `hmbtn_host_patch()` in `cmake/host.cmake` writes a copy of that file under `build/host/host_patched/` with the `static` removed, and the host compiles the copy. The file under `src/` is not edited; the configure fails if the replaced text disappears.

Alternative considered: compiling overlays as unlinked object libraries. Rejected because the host-build spec requires the decompiled sources to be linked into the one executable, and the function-name scan does not show a collision that forces that split.

### 4. Stubs are a compile-driven surface, starting at `libgpu.h`

Headers declare types whose members the sources name, and functions the sources call. Bodies return zero, NULL, or a caller-provided pointer, and they do not touch a device. Symbols nobody calls are not declared.

`libgpu.h` is the first header because `prim.h` and `main()` already name its types. The starting shape, extended only when a compile fails:

```c
#ifndef LIBGPU_H
#define LIBGPU_H

#include <sys/types.h>

typedef struct {
    short x, y, w, h;
} RECT;

typedef struct { u_long tag; u_char r0, g0, b0, code; short x0, y0, w, h; } TILE;
typedef struct { u_long tag; u_char r0, g0, b0, code; short x0, y0; u_char u0, v0; u_short clut; short w, h; } SPRT;
typedef struct { u_long tag; u_long code[2]; } DR_MODE;
typedef struct {
    u_long tag;
    u_char r0, g0, b0, code;
    short x0, y0;
    u_char u0, v0; u_short clut;
    short x1, y1;
    u_char u1, v1; u_short tpage;
    short x2, y2;
    u_char u2, v2; u_short pad1;
    short x3, y3;
    u_char u3, v3; u_short pad2;
} POLY_FT4;

typedef struct { RECT clip; short ofs[2]; RECT tw; u_short tpage; u_char dtd, dfe, isbg, r0, g0, b0; } DRAWENV;
typedef struct { RECT disp; RECT screen; u_char isinter, isrgb24, pad0, pad1; } DISPENV;

int ResetGraph(int mode);
int DrawSync(int mode);
void SetDefDrawEnv(DRAWENV *env, int x, int y, int w, int h);
void SetDefDispEnv(DISPENV *env, int x, int y, int w, int h);
int SetGraphDebug(int level);
int SetVideoMode(int mode);

#endif
```

`DRAWENV` in the real SDK embeds a `DR_ENV`. The host struct keeps the members `main()` writes (`ofs`) and drops the embedded environment until a source names it. `src/pc_port/src/libgpu.c` defines the functions above as returns of zero.

The same rule fills `libgte.h` (`InitGeom`, `SetGeomOffset`, `SetGeomScreen`, `SVECTOR`, `VECTOR`, `MATRIX`), `libgs.h` (`GsInitGraph` and the other `Gs*` calls in `main()`), and the remaining headers. `sys/types.h` typedefs `u_char`, `u_short`, `u_long`, and `u_int` to the 8/16/32/32-bit types from `stdint.h`.

### 5. Shadow the two MIPS headers

`src/pc_port/include/gte.h` defines the same `gte_*` macros as `((void)0)`. `src/pc_port/include/cpu_asm.h` keeps `SCRATCHPAD`, `SCRATCHPAD_SIZE`, and `SCRATCHPAD_STACK_TOP`, and defines `SCRATCHPAD_STACK_ENTER`, `SCRATCHPAD_STACK_LEAVE`, `SAVE_RETURN_ADDRESS`, and `pollhost` as empty statements. The PS1 copies in `src/include/` stay the ones the matching build compiles.

## Risks / Trade-offs

- [A global data symbol collides across overlays] → The function scan did not find one. If the link reports a duplicate, localize or rename it from `src/pc_port/` (wrapper or compile-only flag). Do not edit the game file to make the host link.
- [`u_long` left as `unsigned long` is 64 bits on LP64] → The host `sys/types.h` typedefs it to `uint32_t`. Structs that the sources never name can still have the wrong padding; that is acceptable until a later phase reads packet bytes.
- [Someone expects the game to run] → `ps1_main` is linked but `main_pc.c` does not call it. It still stores through PS1 addresses such as `0x801BE1B0`; `pc-port-ps1-memory` maps them and `pc-port-vsync-loop` calls it.
- [The host branch accidentally becomes the default] → `HMBTN_HOST` defaults to OFF and returns before `psx-toolchain.cmake`. The existing CI command does not pass the option.
- [Stub structs drift from what a source writes] → Add the member when the compiler names it. Do not pre-copy the SDK header.

## Migration Plan

1. Add the `HMBTN_HOST` branch and `cmake/host.cmake` with an empty stub library so the host target configures.
2. Add the include tree and the `libgpu.h` shape above, then compile and append each missing declaration or definition.
3. Shadow `gte.h` and `cpu_asm.h` when those translation units fail on `__asm__`.
4. Stop when `hmbtn-host` links. Rollback is leaving `HMBTN_HOST` unset; the new files are unused by the PS1 build.

## Open Questions

None. Whether the first Windows build uses MinGW GCC or Clang is an environment detail; both are in scope and take the same flags.
