# Proposal

## Why

A page-and-UV BMP covers every primitive that uses that page and those UV bounds. The door capture (`f8-001.bmp`, farm, `place 1`, `scene 0x0`) logged the shipping bin's sample again: page `320,0`, UV `246,245`, CLUT `480,246`, `raw 0x0000`, `color 0x0000`, `LoadImage` at `(368, 245)`. One file for that key would paint the bin and any other primitive the dumper attributes to the same key. The chest and bed dumps used pages `832` and `960`, not `320`, so those three objects are not one shared coordinate.

## What Changes

- A textured primitive drawn for one TMD samples `tmd_<address>.bmp` when that file is the only match for the TMD header address active while its packet was built.
- A second TMD that happens to emit the same page and UV keeps its own file, or VRAM when it has none. The page-and-UV lookup runs only after the model lookup misses.
- The host logs each new TMD address once per session so a file can be named for the door and another for the shipping bin. No texel color is invented, and nothing is written into `vram[][]`.
- `model_draw_tmd_object` stays in `src/`. It is static and links the ordering table itself. The host observes the public draw calls and the existing `debug_hook_run(0x2000)` through the linker.

## Capabilities

### New Capabilities

- None.

### Modified Capabilities

- `host-texture-mods`: A model file is tried before the page-and-UV file. Two models that share a page and UV can take two BMPs. A miss still follows the existing page-and-UV rules, including "no match samples VRAM" and "VRAM is not written".

## Impact

- Host only: `src/pc_port` and `cmake/host.cmake`. Linker wraps for `model_draw`, `model_draw_visible`, and `debug_hook_run`. `texture_resolve` gains a model key. The mods folder stays `mods/textures` or `HMBTN_MODS`.
- No edits under `src/main`, `src/overlay`, or `src/include`. The game global `current_tmd` is not written. `model_draw_visible` does not set it today, so the host keeps its own pointer for the duration of the call.
- Out of scope: painting the farm, PNG, seasonal clothes, moving a page or UV, making `item_art_address` return non-null for an id absent from `resource_lists`, and treating item `0x064` as the chest. The chest has no item id. A live screenshot of the door and the bin stays a later check once the two files exist.
