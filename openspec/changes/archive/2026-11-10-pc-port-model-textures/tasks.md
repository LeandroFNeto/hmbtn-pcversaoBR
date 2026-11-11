# Tasks

## 1. Packet range

- [x] 1.1 Record a primitive-buffer range for one TMD pointer and look a packet address up in it. A self-check shows an address inside the range returns that pointer, an address outside returns none, and a cursor that moves below the range drops it. `git diff --stat -- src/main src/overlay src/include` is empty.

## 2. Draw wraps

- [x] 2.1 Wrap `model_draw`, `model_draw_visible`, and `debug_hook_run` from `cmake/host.cmake`. The draw wraps set the host pointer for the call and clear it on return. Code `0x2000` opens a range at `current_frame->primCursor` only while that pointer is set, and a null frame does not crash. The host links, and the game global `current_tmd` is not written.

## 3. Model file

- [x] 3.1 Load `tmd_` plus eight lowercase hex digits from the same mods folder, sharing the cap of 32 with page files. `texture_resolve` tries that file first. The self-check uses two fixed pointers and one shared page and UV: each model's solid color wins over a page file of another color, a model with no file samples VRAM, two files or an unreadable file for one address do not replace, and the VRAM word is unchanged.
- [x] 3.2 Pass the packet address from the draw walk into that lookup. The self-check draws one constructed primitive inside a range and one outside it, and the pixels follow 3.1. The first use of an address logs `modeltex: tmd_<8 hex digits>` once, and a second primitive for that address does not log it again.

## 4. README

- [x] 4.1 After the self-check passes, the README diagnostic says a `tmd_<8 hex digits>.bmp` in the mods folder is tried before the page-and-UV file, and that `modeltex:` in the log is the file name. The page-and-UV sentence stays.

## Workflow follow-up

- The door and the shipping bin get their file names from `modeltex:` lines captured in front of each. The BMPs are placed by hand. This change does not paint them.
