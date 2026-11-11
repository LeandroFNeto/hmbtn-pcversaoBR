# Design

## Context

See proposal.md for why the house and the flashback are still black. The specs in this change say what a rate-2 readback and a background record must show.

What the host does today, and what it does not:

- Rate 2 is selected by tpage bits 5–6 (`st_abr`). `emit_prim` splits a batch when the rate is 2, and `flush_verts` sets `GL_FUNC_REVERSE_SUBTRACT` for that batch. The fragment writes the source color with alpha 0. With `GL_ONE, GL_ONE_MINUS_SRC_ALPHA`, reverse subtract is destination minus source. Rate 0, 1, and 3 stay on `GL_FUNC_ADD`. The fragment treats rate 1 and rate 2 the same; only the equation differs.
- `vram_check.c` proves rate 2 only with a black source over red, and expects the red to remain. Destination minus zero and destination plus zero are the same pixel, so that check does not tell subtraction from addition. A white source does.
- The flashback line for `place 54`, `scene 0xe6` was `trace gpu: black center ... cmd 0x2a untextured rgb 255,255,255`. `TracePrim` does not store the semi bit or `st_abr`, and `trace_log_flashback` runs for the region name `flashback background` or for scene `0xe7`. Scene `0xe6` with a full black frame is named `black center` because fewer than four samples were non-black. The semi bit and the rate were never printed.
- `glColorMask` and `glStencilMask` are never called. Command `0xE6` is counted and not applied. Commands `0xE3`, `0xE4`, and `0xE5` are not applied. Command `0x02` is noted during `HMBTN_TRACE` and is not drawn.
- `ClearImage` in `vram.c` fills VRAM. When the rectangle overlaps `x < 320` and `y < 480`, it calls `gpu_clear_screen`, which clears the whole window. `gpu_clear_screen` does not receive the rectangle. `ClearImage2` calls `ClearImage` inside `vram.c`, so a wrap of `ClearImage` alone misses a game call to `ClearImage2`.

## Goals / Non-Goals

**Goals:**

- Prove rate 2 with a white source. Change the equation only when that pixel is not the difference.
- On one armed F8, print the last primitive at five points, each large untextured quad, every clear, and every mask word. Print the color write mask and the stencil write mask as they already are.

**Non-Goals:**

- Applying `0xE3`, `0xE4`, `0xE5`, or `0xE6`. Drawing command `0x02`. Calling `glColorMask` or `glStencilMask`. Narrowing `gpu_clear_screen` to the rectangle. Recoloring a rate-2 result that is already the difference. Editing `pc-port-scenery-fixes`, `gte.c`, or `scene.c`.

## Decisions

### White over white is the rate-2 proof

The new check clears to `255,255,255`, draws a semi-transparent untextured quad of `rgb 255,255,255` with `getTPage` rate 2, and reads the pixel. It then clears to `40,0,0` and draws the same quad. The difference is `0,0,0` in both cases. A readback of `255,255,255` means the batch added or the source was dropped; the equation or the fragment output in `libgpu.c` is corrected and the same pair is drawn again. A readback that is already `0,0,0` is not touched.

The black-source check in `vram_check.c` stays. It is not this proof. Rate 0 stays the blend already measured (`137,127,127` for `rgb 20,0,0` over white). Rate 1 and rate 3 are not given a new formula.

Alternative considered: treat the flashback's black pixel as proof that the host subtracts. That pixel has no recorded rate, so it is not the proof.

### The record is armed by scene, not by the black-blob name

`HMBTN_BG_TRACE=1` arms the record. A rising F8, the same edge that already writes `f8-NNN.bmp`, records when the scene is `0x50`–`0x5B`, or when the place is `54` and the scene is `0xe6` or `0xe7`. Any other scene writes one `bgtrace: skip` line and no edge block. Holding F8 does not record the next frame. `HMBTN_TRACE` is not required. The region classifier is not reused: it has already called a farm fade a flashback and missed scene `0xe6`.

The five points are `(40,40)`, `(280,40)`, `(40,200)`, `(280,200)`, and `(160,120)`, in the same coordinate space as the packet vertices. The hit is the last primitive whose triangle contains the point. Each noted primitive stores the semi bit and `st_abr` from the moment it was decoded. The line also includes the pixel read back at that point, so a white packet over a black pixel is visible in one line. The read does not write the pixel.

A large untextured quad is one with depth 0 whose width is at least 160 or whose height is at least 120. Smaller quads stay out of that list. The edge line can still name a small quad when it is the last primitive on a point.

### Clears and mask words share one order with the primitives

`--wrap=ClearImage` and `--wrap=ClearImage2` live in `cmake/host.cmake`. Both wrappers are in `libgpu.c`. Each logs and calls `__real_*`. While the record is armed, a clear appends `x y w h rgb` and whether the rectangle overlaps `x < 320`, `y < 480`. That overlap is the condition that already clears the whole window. A sequence number is taken by each clear and each noted primitive. A point whose last primitive has an earlier sequence than a window clear says `clear after`. At most 32 clears are printed, then one overflow count. With the flag off, the wrappers only call through.

Command `0xE6` appends its word. At the F8, the record reads `GL_COLOR_WRITEMASK` and `GL_STENCIL_WRITEMASK` and prints them. It does not set them. Commands `0xE3`, `0xE4`, `0xE5`, and `0x02` are printed (word, or rectangle and color) and are not executed.

Alternative considered: log only inside `gpu_clear_screen`. That loses the rectangle and loses a clear that does not overlap the display. The spec asks for every cleared rectangle.

### The self-check drives the record without a played scene

`bg_trace_self_check` builds one ordering table: a clear, one `0xE6`, a large white semi-transparent quad at rate 2 over `(160,120)`, and nothing over `(40,40)`. It calls the same formatter with a forced house scene. The check reads the record, not a scraped log line: the center names the white quad and its rate, `(40,40)` is none, the clear rectangle is present, and the mask word is present with the color write mask unchanged. The live house frame and the live flashback are not part of this check.

The check is a host file under `src/pc_port`. The recorder, the wrappers, and any equation change stay in `libgpu.c`. `main_pc.c` calls the check from the existing self-test chain. `src/main`, `src/overlay`, and `src/include` are not edited.

## Risks / Trade-offs

- [A correct rate-2 white quad is black] → The self-check that reads `0,0,0` does not change the equation. The live line is pasted and the pixel stays black. The scenery requirement is not marked done.
- [A window clear after the quad is the black pixel] → The point line says `clear after`. This change does not narrow `gpu_clear_screen`. A later change can, once the line exists.
- [`0xE5` is ignored, so a point test can name a primitive the game shifted away] → The same record prints the ignored `0xE5` word. The offset is not applied here.
- [Scene `0xe7` may not appear] → Scene `0xe6` is the frame already seen and is accepted. A missing scene leaves that capture task open.
- [The wrap runs on every clear] → With the flag off it only calls through. The self-check arms the recorder itself and does not need the environment variable.

## Migration Plan

The flag defaults off. No save or disc format changes. Removing the wraps and the `bgtrace:` lines returns the previous picture, including a rate-2 equation that was not modified.

## Open Questions

None. Whether the live white quad is rate 2 is the F8 record, not a choice this design can make.
