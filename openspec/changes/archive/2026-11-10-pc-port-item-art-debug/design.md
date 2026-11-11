# Design

## Context

See proposal.md for why. The requirements are in `specs/host-item-art-trace/spec.md`.

`item_art_address` (`picture_cells.c`) returns null in four exits: `farm_area.mode.data` is null, `resourceSet` is -1, `resource_lists[resourceSet]` is null, or the id is not in that list. The list's first word is the scan length. On a hit it returns `farm_area.itemArt + offset`, and the offset is the word of `farm_item_tocs[resourceSet]` that walks in step with the list. `picture_load_to_vram` calls it once. A null result reads `0x3C19000 + (id << 11)` from `A_FILE.BIN` and uploads that sector through `texture_pack_load_at`.

`farm_item_tocs` entries are `AnimPackToc`. The host already compiles `anim.h` with `s32 start[]` instead of `s32 start[0]`, and the farm-table self-check already requires every item table's first offset to be 0. `hmbtn_host_patch` writes a copy under `build/host/host_patched/` and fails configure when the searched text is missing. A replacement that contains `;` has to be one quoted CMake argument.

The shipping bin is item `0x064`. The house chest has no item id in `items.h`. House scenes are `farm_area.mode.script.head.scene` from `0x50` through `0x5B`.

## Goals / Non-Goals

**Goals:**

- One log line per watched miss, from the same exits the lookup already has.
- `HMBTN_SELFTEST=1` checks those exits without a disc and without a play session.

**Non-Goals:**

- A second patch of `anim.h`, or any change to `count` or `start`.
- Drawing the bin or the chest. The disc read stays.

## Decisions

### 1. Note the null result at the one caller

`picture_load_to_vram` is the only caller that follows a null with the disc read. The patch adds `#include <item_art_trace.h>` after `#include "picture_cells.h"`, and replaces `item_art_address(id)` with `item_art_note(id, item_art_address(id))`. `item_art_note` returns that pointer, so the assignment and the following `if (address == 0)` are unchanged. Both strings are quoted. The copy under `src/` is not edited. A semicolon in the middle of a `hmbtn_host_patch` argument is split when the argument is forwarded, so the note is the call around the lookup rather than a second statement.

The note is not placed inside `item_art_address`. That function has several `return 0;` lines, and a text replace would hit every one in the file.

Alternative considered: wrap `item_art_address` itself. Rejected: the disc path is the caller, and the globals the lookup just read are still the ones the note needs.

### 2. Classify by re-reading those globals, in the same order

`item_art_note` returns immediately when `address` is not null. Otherwise it classifies, and logs only for id `0x064` or when `farm_area.mode.script.head.scene` is in `0x50`..`0x5B`. A static bit for each id `0`..`1023` keeps the second call quiet. An id outside that range is not logged.

The exits, first match:

1. `no-mode` — `farm_area.mode.data` is null
2. `no-set` — `resourceSet` is -1
3. `set-range` — `resourceSet` is outside 0..9. The note does not index `resource_lists` or `farm_item_tocs`
4. `null-list` — `resource_lists[resourceSet]` is null
5. `absent` — the id is not among the `resource_list[0]` entries that follow the count word

For `absent`, the line includes the count word, `farm_item_tocs[resourceSet]`, `count`, and `start[0]` from `ANIM_PACK_START`. When `count` is outside 1..63, `start` is printed as `unread` and is not indexed.

```
itemart: id 0x064 exit absent list_count 35 toc 0x........ count 35 start 0
itemart: id 0x064 exit no-mode
itemart: id 0x064 exit set-range 12
itemart: id 0x064 exit absent list_count 35 toc 0x........ count 0 start unread
```

The classifier takes the mode pointer, the resource set, the list, the toc, and the id, and returns the line. The self-check calls it with those arguments. The live note only supplies the globals. No allocation.

### 3. Leave the flexible array alone

A bad `count` or a bad `start[0]` shows up on the `absent` line. A good `start[0]` of 0 with exit `absent` means the offsets are present and the id was not in `resource_lists`. That is a different fix from the one in `pc-port-farm-load`, and this change does not pick it.

## Risks / Trade-offs

- [The chest is a model and never calls `picture_load_to_vram`] → The house line is missing from the log. The task records that, and no chest id is invented.
- [The note's re-read disagrees with a local the function already copied] → The note runs on the next line, before any of those globals are written.
- [More than the watched ids miss on the farm] → Only `0x064` and house scenes log, one line each.
- [The searched text gains a second copy] → Configure replaces every copy. The task checks the patched file has one note.

## Migration Plan

Rebuild the host. Saves and the disc image are unchanged. Rollback is removing the `hmbtn_host_patch` entry and the host note.
