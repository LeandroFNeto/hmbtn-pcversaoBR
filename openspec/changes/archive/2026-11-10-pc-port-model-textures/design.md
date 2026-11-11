# Design

## Context

See proposal.md for why a page-and-UV file cannot separate two models. `model_draw` (`src/main/geometry/model.c`) sets the game global `current_tmd` and calls static `model_draw_tmd_object`, which writes packets into `current_frame->primCursor` and links them itself. `debug_hook_run(0x2000)` runs at the start of each object. `model_draw_visible` draws the same way and does not assign `current_tmd`. `DrawOTag` runs later, after both functions have returned. `texture_resolve` today only sees page and UV (`mods_lookup` in `src/pc_port/src/mods.c`).

## Goals / Non-Goals

**Goals:**

- Stamp each packet built inside `model_draw` or `model_draw_visible` with the TMD header pointer that was live while it was built.
- Let `texture_resolve` prefer `tmd_<8 hex digits>.bmp` for that pointer, then the existing page-and-UV file, then VRAM.
- Log each pointer once per session.

**Non-Goals:**

- Editing `model_draw_tmd_object` or writing `current_tmd`.
- A file name based on item `0x064`. That id is not on the packet, and the chest has no item id.
- Generating door or bin pixels.

## Decisions

### Stamp a primitive-buffer range, do not keep a global across DrawOTag

A host static set on entry to `model_draw` and cleared on return is empty when `DrawOTag` replays the ordering table. The stamp has to live with the packet.

`model_draw_tmd_object` is static, so the linker cannot wrap it. It already calls `debug_hook_run(0x2000)` before taking `primCursor`. The host wraps that function. While a host pointer is set, code `0x2000` closes the previous range at the current cursor and opens a range from that cursor. The `model_draw` and `model_draw_visible` wraps set the pointer from `model->tmd` or `obj->tmd`, call the real function, then close the open range at the cursor and clear the pointer.

Alternative considered: wrap only `ot_insert_prim`. The object loop does not use it. The two `ot_insert_prim` calls are the separate quad helpers. A range closed at return still covers quads emitted before the wrap returns, and they belong to the same TMD header.

Alternative considered: write `current_tmd` from the host. `model_draw_visible` would still leave it stale, and writing the game global is a change of game state.

### Lookup happens from the packet address

`decode_poly` and `decode_rect` already have the packet pointer. The resolve call passes that pointer. A packet inside a live range uses that range's TMD. No range means the page-and-UV path, unchanged.

Ranges are dropped when `primCursor` for that frame moves below a range start, and after `DrawOTag` finishes that ordering table. The two frame buffers reuse their primitive storage.

### One mods folder, model key first

`mods.c` parses `tmd_` plus eight lowercase hex digits as a model slot, separate from `page_u` parsing. The existing scan skips a name that is not six integers, so a model file is not a page key. The folder stays `SDL_GetBasePath()+"mods/textures"` or `HMBTN_MODS`. `SDL_LoadBMP` only. The GL texture is `GL_RGB`, nearest, clamp. Nothing is written into `vram[][]`. Model slots and page slots share the existing cap of 32. Two readable files for one address, or an unreadable file, leave that address with no model texture. `texture_resolve` tries the model slot first.

### The log line is the file name

The first time a range is opened for an address, the host logs `modeltex: tmd_<8 hex digits>`. The user names the door file and the bin file from those lines. The address is whatever the loader used this session. The self-check uses two fixed pointers and does not depend on the disc.

## Risks / Trade-offs

- [A packet built outside the two draw functions has no range] → It keeps the page-and-UV rules. The door and the bin go through `model_draw` from the farm scene list.
- [Quads linked after the last `0x2000` and before the wrap returns share the last object's TMD] → They are the same header. A quad drawn with the host pointer clear stays on page and UV.
- [The primitive buffer is reused] → Ranges die when the cursor rewinds or when `DrawOTag` finishes that table.
- [The load address can differ on another launch] → The log prints the address for that session. The self-check does not claim the disc address is stable.
- [The chest and the bed did not sample page `320`] → This change does not assert they share the bin UV. A later file for their own logged address is the same mechanism, not a separate patch.

## Migration Plan

Page-and-UV files already in `mods/textures` keep matching. Adding a `tmd_` file does not move them. Removing the wraps restores the current resolver. No disc or save migration.

## Open Questions

None. The intercept, the file name, and the precedence are fixed by the spec.
