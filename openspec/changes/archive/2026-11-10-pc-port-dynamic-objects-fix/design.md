# Design

## Context

See proposal.md for why. The requirements are in `specs/host-item-picture/spec.md`.

`item_art_address` returns null for an id that is not in `resource_lists[resourceSet]`. On that null, `picture_load_to_vram` reads `0x800` bytes at `0x3C19000 + (id << 11)` and binds the buffer as an `.ANI`. An `.ANI` starts with magic `0xE` (`anim.c`). The 14×1 `LoadImage` in the farm log is one row of `texture_pack_upload`: a pack whose type byte is `0x0D`, with flag bits 2–3 clear, sets `rect.h = 1` and calls `LoadImage` per row. `texture_pack_load_at` is the public entry. `anim.h` on the host already uses `s32 start[]` (`pc-port-farm-load`). The spring list's count word is 35 and `farm_item_toc_0.count` is 35, with `start[0] == 0`. Id `0x064` is in none of the ten lists. `items.h` names `0x05E` as `ITEM_RED_GRASS` and `0x064` as `ITEM_BASKET`. The chest has no item id.

## Goals / Non-Goals

**Goals:**

- The self-check fails when a non-zero source row is stored as `0x0000`.
- The self-check names the real sector for `0x064` before any upload change.
- One upload result, chosen by that sector line.

**Non-Goals:**

- A second patch of `anim.h` or a host copy of `items.h`.
- A non-null return from `item_art_address` for an id the list does not contain.
- A TMD magic at that pointer. The pointer is an `.ANI` or null.

## Decisions

### 1. The self-check uploads one constructed row

`HMBTN_SELFTEST=1` calls `texture_pack_load_at` with a pack in a stack buffer: type `0x0D`, one block, flag bits 2–3 clear, one row, one texel. The first call's texel is non-zero. The second call's texel is zero. Each call is followed by a read of that VRAM address. The non-zero call must not read `0x0000`. The zero call must read `0x0000`. The check does not go through `item_art_address`.

Alternative considered: assert `item_art_address(0x064)` and `item_art_address(0x05E)` are non-null TMD headers. Rejected: the spring log is `exit absent` with matching `count` and `start 0`, `0x05E` is red grass, and a hit returns `farm_area.itemArt + offset`, whose first word is the `.ANI` magic `0xE`.

### 2. The sector line picks the upload result

The same self-check reads `0x800` bytes from the extracted `A_FILE.BIN` at `0x3C19000 + (0x064 << 11)`. The log line is:

```
itempic: id 0x064 magic 0x........ image zero|nonzero
```

`magic` is the first word. `image zero` means every byte after the header the upload would consume is `0x00`. `image nonzero` means it is not.

- **Non-zero picture.** Magic is `0xE` and the line says `nonzero`. The row upload that `picture_load_to_vram` reaches for this file is the one decision 1 already checks. If a farm sample of the bin is still `0x0000` after that upload is faithful, the task pastes the sample and stops. No second theory, and no color is invented.
- **Not a picture, or a zero image.** Any other magic, or `image zero`. Paste the line. Do not change the upload. The picture tasks stay open and do not fail the build.

Alternative considered: always rewrite the VRAM word so the bin is opaque. Rejected: `pc-port-dynamic-tmd-vram` already stopped on a stored `0x0000`, and a zero source is the same stop.

### 3. The farm picture is one capture

After a `nonzero` line and a faithful upload, one capture shows the shipping bin. It samples a non-zero word and is not a transparent hole. The house id `0x11e` is not given its own result. A capture that still shows it as a hole is pasted and that task stays open.

## Risks / Trade-offs

- [The byte offset is a sector index inside the disc image, not the raw file] → The line's magic is `0xE` only for the offset the game reads. A different magic stops decision 2. The offset is not adjusted to force `0xE`.
- [The bin's TMD samples a page this upload never writes] → Decision 2's non-zero branch pastes that sample and stops. The page is not relocated in this change.
- [`texture_pack_load_at` returns without writing when the type byte is not `0x0D`] → Decision 1's pack uses `0x0D`. A silent return fails the non-zero check.

## Migration Plan

Rebuild the host. Saves and the disc image are unchanged. Rollback is reverting the host check and, if an upload fix landed, that host change. No `hmbtn_host_patch` unless the faithful upload cannot be reached from `texture_pack_load_at` alone.

## Open Questions

None. The sector line picks the result.
