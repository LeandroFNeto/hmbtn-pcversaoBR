# Proposal

## Why

The shipping bin draws as a black square because the texel it samples was stored as `0x0000`. The newest `LoadImage` covering that word is a 14×1 row, the shape `texture_pack_upload` writes one decoded line at a time, and `picture_load_to_vram` takes that disc path only when `item_art_address` returns null. The sector it then reads is `0x3C19000 + (id << 11)`. This change records which exit of that lookup produced the null, and the table words it read, before any offset is patched again.

## What Changes

- A host patch on the copy of `picture_cells.c` notes a null result of `item_art_address` for the shipping bin (`ITEM_BASKET`, `0x064`) and for a null result while `farm_area.mode.script.head.scene` is a house scene (`0x50` through `0x5B`, every house level and season). `items.h` has no chest item id, so the house line names the id that actually missed.
- The line names the id, why the lookup returned null, the `farm_item_tocs` entry it indexed, that entry's `count`, and `start[0]`. It does not change the pointer the game uses and does not skip the disc read.
- `AnimPackToc.start[0]` is already a flexible array on the host (`pc-port-farm-load`), and `HMBTN_SELFTEST=1` already checks all 10 item tables. This change does not patch `anim.h` again. The log is what says whether `count` or `start[0]` is still wrong, or whether the id was simply absent from `resource_lists`.
- No pixel changes and no edits under `src/main`, `src/overlay`, or `src/include`. Filling the sprite is a later change, after the pasted line names the exit.

## Capabilities

### New Capabilities

- `host-item-art-trace`: A null item-art lookup for the shipping bin, or a null lookup in the house, writes one line with the id, the exit, and the table-of-contents words that were read.

### Modified Capabilities

- None. `item_art_address` still returns the same pointer, and the disc fallback still runs when that pointer is null.

## Impact

- `cmake/host.cmake`: one `hmbtn_host_patch` of `src/overlay/11_GF_FARM/picture_cells.c`, adding the note at the null check inside `picture_load_to_vram`.
- `src/pc_port`: the note and a self-check. The log stays `build/host/hmbtn-host.log`.
- The README diagnostic paragraph names the line. The live check is a farm load with the bin on screen, and the house if the chest is visited, with the line pasted into the task.
- Out of scope: changing a table offset, skipping the disc read, inventing a texel color, MDEC, and the Linux build.
