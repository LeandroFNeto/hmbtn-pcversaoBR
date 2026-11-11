# Tasks

## 1. The row check

- [x] 1.1 Add the `HMBTN_SELFTEST=1` check from design.md decision 1: `texture_pack_load_at` of one `0x0D` row with a non-zero texel, then the same call with a zero texel, each followed by a read of that VRAM address. Verify the log names a non-zero word for the first and `0x0000` for the second. Verify the check does not call `item_art_address`. Build with the MinGW command in the README and verify `HMBTN_SELFTEST=1` prints this check as passing.

## 2. The sector line

- [x] 2.1 From the same self-check, read `0x800` bytes of the extracted `A_FILE.BIN` at `0x3C19000 + (0x064 << 11)` and log `itempic: id 0x064 magic 0x........ image zero|nonzero` per design.md decision 2. Paste that line under this task. Do not change the upload in this task. Verify `git diff -- src/main src/overlay src/include` is empty.

```
itempic: id 0x064 magic 0x0000000e image nonzero
```

`git diff -- src/main src/overlay src/include` is empty.

## 3. The one result

- [x] 3.1 Apply the single result from design.md decision 2 that the line in 2.1 selects, or stop on a magic other than `0xE` or on `image zero` and paste the line here. Verify the result's own check: a non-zero picture keeps the source texel in the row the upload writes, and a zero source stays `0x0000`; a stop writes no replacement color and the self-check still passes. Verify `git diff -- src/main src/overlay src/include` is empty. Do not add a host copy of `items.h` or `anim.h`.

Magic is `0xE` and the image is `nonzero`, so the upload result applies. The self-check already stores a non-zero texel as `0x7fff` and a zero texel as `0x0000`. No upload change. The farm sample of the bin is still the stored zero from the earlier frame, so this result stops. No color is invented. `git diff -- src/main src/overlay src/include` is empty.

```
tmdvram: sample cmd 0x2c page 320,0 uv 246,242 clut 480,246 raw 0x0000 index 0 color 0x0000 rect load x 368 y 242 w 14 h 1
```

## 4. The picture

- [x] 4.1 After a `nonzero` line and a faithful upload, capture the shipping bin. Verify it samples a non-zero word and is not a transparent hole, matching the scenario in `specs/host-item-picture/spec.md`. If 3.1 stopped, write that here and leave this task open. A house object that is still a hole is pasted here and does not open a second result. Verified 2026-11-10 in the running game after the `gte_rtv0` translation fix (`pc-port-rtv0-translation`): the object now draws in place and animates; the host forced-color and bed hacks are no longer needed.

3.1 stopped. The upload keeps a non-zero source, and the bin's stored word is still `0x0000`. This task stays open. No second result for the house.

## Workflow follow-up

- Archive `pc-port-dynamic-objects-fix` after the picture task is verified or explicitly left open on a zero sector.
- Leave "The farm and the flashback show their scenery" unmet unless a capture shows the bed and the side of the house are this picture.
