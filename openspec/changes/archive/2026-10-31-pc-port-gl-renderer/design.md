# Design

## Context

The host target links `main_pc.c` and `libgpu.c`, opens a 640×480 SDL window, and returns on `SDL_QUIT`. `DrawOTag` does not exist in the stub yet. The game never reaches that stub in this change: `ps1_main` is not called, and no file under `src/main/` or `src/overlay/` is on the host link.

When the game does draw, `main_draw_ots` in `main.c` is the only submit. It calls `PutDispEnv`, `PutDrawEnv`, and `DrawOTag` on the ordering tables. Packets are already in screen space. `model.c` fills those coordinates with GTE macros (`gte_rtps`, `gte_rtpt`, `gte_stsxy`) and links `POLY_F4`, `POLY_FT4`, and `POLY_GT4`. UI code also builds `SPRT` and other commands. `src/include/gte.h` is MIPS `.word` encodings, included only by `model.c` and `vec_length.c`.

The draw environment created in `main` is 320×240. The window is 640×480.

## Goals / Non-Goals

**Goals:**

- One `DrawOTag` implementation in `libgpu.c` that the game can call later and that a static host packet calls now.
- A software GTE whose screen X/Y match the packet fields `gte_stsxy` writes.
- An OpenGL 2.0 context created by SDL, with shaders loaded through `SDL_GL_GetProcAddress`, linked only to `opengl32`.
- `cmake --build` of the current host target stays successful after every step, using the existing `SDL2_DIR`.

**Non-Goals:**

- Sampling VRAM, CLUTs, or PNG files.
- Calling `ps1_main`, compiling game sources, or drawing `LINE`, `TILE`, and `DR_MODE`.
- Reproducing every GTE limiter, flag bit, and IR saturation. The first projection is the matrix, translation, offset, and divide used above.

## Decisions

### 1. Intercept the submit, not the constructors

`DrawOTag` is the intercept. Constructors (`prim_make_rect`, `model_draw_quad_depth`, `addPrim`) keep building the same bytes. `libgpu.c` walks from the tag the caller passes.

Each tag word is a length in the top byte and a next address in the low 24 bits. Length zero is a bare ordering-table link. Otherwise the packet follows the tag. The command byte is the fourth byte after the tag. Bit 1 is semi-transparency and is ignored for matching, so `0x28` and `0x2A` are both a flat quad. This change draws:

| Code (bit 1 cleared) | Packet | Triangles |
| --- | --- | --- |
| `0x28` | `POLY_F4` | one color, four corners |
| `0x2C` | `POLY_FT4` | one color, four corners; UV ignored |
| `0x3C` | `POLY_GT4` | a color at each corner; UV ignored |
| `0x64` | `SPRT` | one color, axis-aligned rectangle from x, y, w, h |

Any other code advances by the tag length and continues. A null next address ends the walk.

A PS1 quad is stored as corner 0, 1 across the top and corner 2, 3 across the bottom. The two triangles are (0, 1, 2) and (1, 3, 2).

The demo packet is a static `POLY_F4`, not a heap block. On this MinGW image the executable base is `0x00400000`, so a static address fits in 24 bits and survives the mask the game's `LINK_PRIM` uses. `main_pc.c` calls `DrawOTag` on that packet after the clear and before the swap.

### 2. GTE writes pixels; OpenGL only scales them

The game stores screen pixels in the packet, then `DrawOTag` reads pixels. The host GTE is therefore a C transform, not a GL matrix. OpenGL draws a 320×240 pixel space into the 640×480 window with an orthographic scale of 2 and a flipped Y (PS1 Y grows downward).

For a vertex `(vx, vy, vz)`, a 3×3 rotation whose elements are 1.3.12 (4096 means 1), a translation `(tx, ty, tz)`, offset `(ox, oy)`, and screen distance `h`:

```
x = ((m00*vx + m01*vy + m02*vz) >> 12) + tx
y = ((m10*vx + m11*vy + m12*vz) >> 12) + ty
z = ((m20*vx + m21*vy + m22*vz) >> 12) + tz
```

If `z` is not positive, the front-of-camera flag fails and no screen coordinate is stored. Otherwise:

```
sx = ox + x * h / z
sy = oy + y * h / z
```

`sx` and `sy` are stored as signed 16-bit values, which is what `gte_stsxy` writes. `SetGeomOffset` and `SetGeomScreen` in the host `libgte` set `ox`, `oy`, and `h`. `main` uses offset `(0, 0)` and distance `1000`; the identity check in the spec uses offset `(160, 120)` so the optical axis is the center of the 320×240 draw area.

The vertex shader receives those pixels and a color. The fragment shader writes the color. There is no sampler. Entry points beyond OpenGL 1.1 (`glCreateShader`, `glUseProgram`, and the buffer calls) come from `SDL_GL_GetProcAddress`.

`src/pc_port/include/gte.h` shadows `src/include/gte.h` for the host include path and defines the macros `model.c` uses as calls into this C implementation. Nothing on the current host link includes that header, so the shadow cannot break the existing executable. The spec's projection check calls the C functions directly.

### 3. OpenGL is the system library

`cmake/host.cmake` keeps `find_package(SDL2 REQUIRED)` and adds `opengl32` on Windows. No GL loader package and no second `SDL2_DIR`. The context is created with `SDL_WINDOW_OPENGL` before `SDL_GL_CreateContext`. Attributes ask for a 2.1 compatibility context, which MinGW's `opengl32` can start and which is enough for these two shaders.

Semi-transparency, texture pages, and draw-environment clip rectangles are stored and not applied. A skipped command is not an error.

## Risks / Trade-offs

- [Heap packets masked to 24 bits] → The demo uses a static packet. Before game sources join the link, the primitive arena has to live in the low 16MB or `LINK_PRIM` has to keep the full pointer. This change does not do either.
- [GTE limits differ from the coprocessor] → Screen position of a simple vertex is the acceptance check. Flag bits other than "not in front" can wait.
- [Textured quads look flat-colored] → UV and CLUT stay in the packet and are not read. That is the later texture change.
- [A 2.1 context is unavailable] → The process logs the SDL error and returns non-zero instead of spinning a blank window.
- [The PS1 configure starts loading host GL] → `opengl32` is inside `cmake/host.cmake`, which runs only when `HMBTN_HOST` is on.

## Migration Plan

1. Add the context and a clear color. Build with the current `SDL2_DIR`. Confirm the window still closes.
2. Add `DrawOTag` and the static flat quad. Build again. Confirm the quad color is on screen.
3. Add the GTE functions and the identity check. Build again. The check returns success without linking `model.c`.
4. Roll back by leaving `HMBTN_HOST` unset. The new files are not on the PS1 path.

## Open Questions

None.
