# Tasks

## 1. Capture

- [x] 1.1 Gate `MoveImage` and ordering-table `0x80` per design.md decisions 1 to 3. When the destination overlaps `(896, 256, 64, 256)` and the source overlaps the current draw clip, call `gpu_refresh_draw_overlap` on the source, then the existing move. Any other copy, and `StoreImage` of the scratch, stay as they are. Add no buffer and no `malloc`. Verify `HMBTN_SELFTEST=1`: an 8 by 8 block of a known color inside the draw buffer lands as that 15-bit color at `(896, 256)` through `MoveImage` and through `0x80`; the same move from the buffer at `(0, 240)` lands the pixels of that frame; a move from outside the draw buffer onto the scratch keeps the source word; a 16 by 1 move at `x = 896` with `y` outside 256 through 511 keeps the source word; `StoreImage` of a scratch filled with a known word, with no capture, returns that word; a drawn frame with no album copy leaves a sentinel in the scratch. The existing check that `StoreImage` of `(896, 256, 64, 256)` is not replaced by the frame still passes. `git diff --stat -- src/main src/overlay src/include` is empty.

- [x] 1.2 After 1.1, a 16-bit sprite that samples the captured cell at `(896, 256)` draws that color, and the capture creates no second texture. The first gated copy in a session logs `album:` with the source rectangle, the destination, and one destination word. Verify `HMBTN_SELFTEST=1` reads that pixel back as the captured color. Update the README in the same turn: the resolved row states that this constructed copy writes the 15-bit color and the sprite shows it; "Ainda falta" keeps an in-game photo until task 2.1, and keeps the black scenery. The page-and-UV sentence and the `tmd_` sentence stay.

## 2. In-game photo

- [ ] 2.1 Take one photo in the game and open the album. The picture is the frame that was on screen. Paste the `album:` line. If that line never appears, the game did not copy the draw buffer into `(896, 256, 64, 256)`: leave this task open and do not invent the picture. `pc-port-scenery-fixes` is not edited.

## Workflow follow-up

- Archive the change after the tracked tasks are done and the result has been reviewed.
