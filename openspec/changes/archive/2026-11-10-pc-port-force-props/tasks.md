# Tasks

## 1. Corner color on the measured holes

- [x] 1.1 In `src/pc_port/src/libgpu.c`, set the design.md decision 1 flag on a textured primitive whose page is `320,0` and whose UV rectangle contains `(246, 242)`, or whose page is `384,0` and whose UV rectangle contains `(222, 159)`, only when `texture_resolve` did not return a BMP. When the flag is set and the sampled color word is `0x0000`, the fragment uses the packet corner color and is not discarded. Any other `0x0000` sample is still discarded. A matching BMP still samples the image. Verify `HMBTN_SELFTEST=1`: the bin-key primitive with corner color `128,128,128` and word `0x0000` reads `128,128,128`; the same word on page `0,0` leaves the cleared color; a BMP for the bin key reads the BMP color; the door key with word `0x0000` reads its own corner color. Verify `git diff -- src/main src/overlay src/include` is empty.

- [x] 1.2 Update `README.md` in the same turn as 1.1. The resolved row states that the shipping bin and the farm door show the packet corner color when the measured sample is `0x0000` and no BMP matches, and that a BMP still wins. "Ainda falta" keeps the bed until group 2, and keeps the black scenery and the chest.

## 2. Bed primitive

- [x] 2.1 Add the host function and the `hmbtn_host_patch` from design.md decision 2. The final `TMD_PRIM_GT4_RAW` mismatch calls the host only for the object nearest `(254, 170)` while that object's drawn count is 0. `TMD_PRIM_FT4` reuses the `TMD_PRIM_FT4_RAW` vertex loads at `0x18`, `0x1A`, `0x1C`, and `0x1E`, the color word at `source + 4`, and the stride `0x20`, and links an untextured quad (`0x28`). A non-positive `nclip` still drops the face. A header with no same-stride RAW twin ends the object as it does today. Verify `HMBTN_SELFTEST=1`: a constructed `TMD_PRIM_FT4` object whose screen point is nearest `(254, 170)` reads its stored color; an object with a header that has no same-stride RAW twin leaves the cleared color; a front face still passes `nclip` and a back face still fails. Verify `git diff -- src/main src/overlay src/include` is empty.

- [x] 2.2 Update `README.md` in the same turn as 2.1. The resolved row states that a constructed bed-sized `TMD_PRIM_FT4` is drawn in its stored color and that the `nclip` sign is unchanged. "Ainda falta" keeps an in-game house view until 3.1.

## 3. One view of the farm and the house

- [x] 3.1 Build the host and capture one frame facing the shipping bin and one inside the house at the bed. Verify the bin and the farm door are not transparent holes, and the bed place is not a solid black rectangle. Paste the `trace object:` line for the bed. If that header is not a same-stride RAW twin, leave this task open and do not add a second code. Verify `git diff -- src/main src/overlay src/include` is empty. Update `README.md`: a capture that shows the three objects moves them to "Já resolvido"; a bed header that stops stays in "Ainda falta" with this task named. Verified 2026-11-10 in the running game after the `gte_rtv0` translation fix (`pc-port-rtv0-translation`): the object now draws in place and animates; the host forced-color and bed hacks are no longer needed.
