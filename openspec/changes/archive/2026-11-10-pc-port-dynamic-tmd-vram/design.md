# Design

## Context

See proposal.md for why. The picture requirement is the two ADDED requirements in `specs/gpu-packet-replay/spec.md`.

What the code does today, as opposed to a stale OpenGL texture:

- `sample_word` reads the CPU mirror `vram[][]`. For a 4-bit primitive it takes the nibble at `page + uv`, then the CLUT entry. The logged `word 0x0000` is that CLUT color. It is already zero before `glTexSubImage2D`.
- `LoadImage`, `MoveImage`, and `ClearImage` write that mirror and mark the rows dirty. `upload_vram` uploads every dirty row as `GL_LUMINANCE_ALPHA`. A copy the host stored cannot be missing from the sample for that reason.
- A `0x3c` packet carries the texture page in the second vertex. `set_tpage` applies that word before the sample. `model_relocate_textures` adds a page offset to the TMD in memory. It does not copy texels.
- The two dumps that selected branch E are the same house frame (`scene 0x50`). The black blob runs from x 72 to 320 and y 0 to 240, so it is not the outline of the chest or the bin. The `word 0x0000` lines are still the evidence. The picture check needs a frame where one object fills the middle.

## Goals / Non-Goals

**Goals:**

- One frame that says whether the zero word was never stored, stored at a different page, or stored as zero.
- Apply that one result so the spec's objects are not transparent holes when the game loaded a non-zero texel.

**Non-Goals:**

- Recoloring a `*_RAW` packet, or painting a color over a word the game stored as `0x0000`.
- Reopening branch A, B, C, or D of `pc-port-invisible-objects`.
- Lines, dithering, and the mask bit.
- Treating the bed and the side of the house as done. "The farm and the flashback show their scenery" stays unmet until a capture shows those pixels are these objects.

## Decisions

### 1. The trace is one frame, and the blob has to be the object

`HMBTN_TMD_VRAM=1` arms a single dump on the next F8 while the view is a farm scene and the frame is not a fade. A black blob shorter than the screen dumps the textured primitives that overlap it. The chest and the door sit in a black region that already runs the full screen height, so that outline is not the object. A full-height blob, and a frame with no blob, dumps the textured primitives that cover the center of the view. The player aims the chest, the door, or the bin at that center. No such primitive leaves the arm set.

For each textured primitive that overlaps the blob, or the center when there is no blob, and whose sample is `0x0000`, the log prints the command, the page, the UV, the CLUT, the raw VRAM word, the 4-bit or 8-bit index, and the CLUT color. It also prints the last `LoadImage`, `MoveImage`, or `ClearImage` rectangle that covers that VRAM address, or `rect none` when no rectangle in the frame's history covers it. The history is the rectangles since the farm scene was entered, kept in a heap buffer of 65536 entries. Once that buffer is full, the newest rectangle overwrites the oldest. It is not a static image.

### 2. One result, chosen by that line

No fix is written before the log is in the task. Exactly one result applies.

- **Stored elsewhere.** A rectangle in the history holds non-zero texels, and the packet's page plus UV does not land inside it. The fix changes the page used for the codes the log names, by the delta the rectangle shows. A code with no size in `model_relocate_textures` stops the result; the header is pasted into the task and no layout is invented. The host patch is the only edit that may touch game source.
- **Never stored.** No rectangle covers the address, and a GPU copy command for that rectangle was issued and did not call the VRAM store. The fix stores that command's rectangle. Commands that already call `vram_load` or `vram_move` are left alone.
- **Stored zero.** A rectangle covers the address and the word it wrote is `0x0000`. Stop. The rectangle and the word are pasted into the task. No color is invented.

### 3. The flashback is not a second result

After the result, the flashback is captured against the second requirement. If the farm objects sample a non-zero word and the flashback is still a hole, the task stays open and that frame's lines are pasted beside the farm log. A new result is not opened for the flashback alone.

## Risks / Trade-offs

- [The next F8 is another full-height house blob] → Decision 1 dumps the textured primitives at the center of that frame. The outline of the blob is logged and is not treated as the object.
- [Index 0 of a real CLUT is transparent by design] → Decision 2's stored-zero result stops. The raw word and the index are both in the line, so a zero nibble is not called a missing page.
- [The outdoor bin and the indoor chest use different pages] → The task captures one object that fills the middle, then the picture check covers the other three. The page delta is applied only to the codes the first log names.
- [The bed and the side of the house are these same pixels] → The older scenery requirement stays open until a capture says so.

## Migration Plan

Rebuild the host. Saves and the disc image are unchanged. Rollback is reverting the host files and, if the stored-elsewhere result landed, the `hmbtn_host_patch` entry.

## Open Questions

None. The rectangle line picks the result, and a stored zero stops.
