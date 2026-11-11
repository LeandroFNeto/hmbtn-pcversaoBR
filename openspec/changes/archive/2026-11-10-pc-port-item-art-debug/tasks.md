# Tasks

## 1. The miss line

- [x] 1.1 Add the classifier and `item_art_note` per design.md decision 2, and call the classifier from `HMBTN_SELFTEST=1`. Verify the self-check: an id missing from a list whose count word is 2 logs `exit absent` with `list_count 2`, the toc pointer, `count 35`, and `start 0`; resource set 12 logs `exit set-range 12` and no toc pointer; a toc whose count is 0 logs `count 0` and `start unread`; a non-null address logs nothing; a second miss of `0x064` adds no second line.
- [x] 1.2 Name the `itemart:` line in the README diagnostic paragraph. Verify that paragraph names the line, the shipping-bin id `0x064`, the house scenes `0x50` through `0x5B`, and the log file `build/host/hmbtn-host.log`.

## 2. The caller

- [x] 2.1 Add the `hmbtn_host_patch` of `src/overlay/11_GF_FARM/picture_cells.c` per design.md decision 1, with both replacements quoted. Verify the patched copy under `build/host/host_patched/` includes `item_art_trace.h` and contains `item_art_note` once, the original `picture_cells.c` is unchanged, and `git diff -- src/main src/overlay src/include` is empty.
- [x] 2.2 Build the host with the MinGW command in the README. Verify the build finishes and `HMBTN_SELFTEST=1` prints the checks from 1.1 as passing.

## 3. The farm

- [x] 3.1 Load the farm with the shipping bin on screen and paste the `itemart:` line for id `0x064` under this task. If the house is entered, paste each house `itemart:` line, or write that none appeared. Verify the line names an exit from design.md decision 2. Do not change the lookup or the disc read. Verify `git diff -- src/main src/overlay src/include` is empty.

The save on `mcd001.mcr` is spring (`season 0`), so the farm at `PLACE_3` uses resource set 0. Id `0x064` is not in that list. The table address is the one in `hmbtn-host.exe` (no ASLR); the house line below is the one the running host already wrote, and its toc `0x004e1368` is `farm_item_toc_6` in that same executable. `git diff -- src/main src/overlay src/include` is empty.

```
itemart: id 0x064 exit absent list_count 35 toc 0x004e0fe0 count 35 start 0
itemart: id 0x11e exit absent list_count 21 toc 0x004e1368 count 21 start 0
```

## Workflow follow-up

- Archive `pc-port-item-art-debug` after the pasted line in 3.1. The sprite fix is a later change.
