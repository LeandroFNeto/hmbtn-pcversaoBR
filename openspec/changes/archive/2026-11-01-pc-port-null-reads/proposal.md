# Proposal

## Why

With the farm's tables fixed (`pc-port-farm-load`), a new game now gets past `farm_bring_up` and then the host faults before the farm draws its first frame: `scene_resolve_selection` (`scene.c:291`) reads `farm_area.mode.data->head.place.value` while `data` is still null. A new game enters the farm through `farm_bring_up`, which never sets `data`; only `farm_startup` and `scene_enter` (ten lines after the read) do. The PS1 reads null the same way, but there address 0 is RAM: the kernel's "garbage area", whose first halfword is 3 (psx-spx, "Garbage Area at Address 00000000h"). 3 is neither `PLACE_MINE_43` nor `PLACE_MINE`, so the console carries on. On the host, page 0 is not mapped and the process dies.

Once that read was covered, the same frame wrote through null: `scene_slot_flag_move` (`scene.c:1265`) updates the flags of two marked scene entries the area does not have, a halfword at 0x28, in the PS1 kernel's unused RAM.

Until this is fixed nothing in the farm can be played, and saving in the diary (`pc-port-memory-card` tasks 4.2 and 4.3) stays out of reach.

## What Changes

- After mapping PS1 RAM at 0x80000000 (the same physical memory the PS1 reaches through address 0), the host writes the low kernel words psx-spx documents there (the garbage area, the RAM size). A `PS1_NULL_PTR(p)` helper yields `p`, or 0x80000000 when `p` is null.
- `hmbtn_host_patch` takes several replacements per file, so one patched copy can include the helper's header and wrap the null accesses.
- `scene.c` is compiled from a patched copy in which the null read of `farm_area.mode.data->head.place` and the null writes through `scene_marked_entries` go through `PS1_NULL_PTR`. The file under `src/` is not edited.
- The crash log lists the call stack (return addresses) with the fault, so the next null access can be found from the log and `addr2line` without attaching `gdb`.
- A new game is run into the farm again. Further null reads, or null writes into the kernel's unused RAM, found there get the same treatment in this change, each listed with its PS1 justification. Any other kind of blocker is recorded and handed back.
- No change under `src/` outside `src/pc_port/`.

## Capabilities

### New Capabilities

- `host-crash-report`: What the host logs when the process faults, so a crash can be located from the log alone.

### Modified Capabilities

- `host-source-compat`: adds that reads and writes through a null pointer the PS1 tolerates reach what they reach on the PS1, without editing the game sources, and widens "A new game reaches the farm" from "no stall in the debug trap" to "no fault or stall".

## Impact

- `src/pc_port/include/ps1_null.h`, the low RAM values in `ps1_memory_map` (`src/pc_port/src/ps1_memory.c`), and their check in `ps1_memory_check.c`.
- `cmake/host.cmake`: `hmbtn_host_patch` with several replacements, and the patch of `src/overlay/11_GF_FARM/world/scene.c`.
- The crash logger (`host_install_crash_logger`).
- `pc-port-memory-card` tasks 4.2, 4.3, and 5.1 become reachable once the farm runs. The open tasks 3.1 and 3.2 of `pc-port-farm-load` (archived) are carried by tasks 4.1 and 4.2 here.
- README: the "A fazenda não carrega" item moves or is replaced by the next blocker.
- Out of scope: a general mechanism that catches every null access at run time (see design.md), null writes onto the kernel words the host seeds, and farm problems that are not null accesses.
