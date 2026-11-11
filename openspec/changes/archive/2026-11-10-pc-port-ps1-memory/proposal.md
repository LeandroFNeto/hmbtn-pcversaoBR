# Proposal

## Why

Once `ps1_main` runs on the host, the game stores through fixed PlayStation addresses: `WORK_REGION` is `0x801BE1B0`, the ordering-table bases are `0x801F7000` and `0x801FCFF8`, the scratchpad is `0x1F800000`, and the overlay regions start at `0x800B7E20` and `0x8012DDE8`. It also links packets with `LINK_PRIM` and `ot_insert_prim`, which keep only the low 24 bits of each pointer. A default Win32 process cannot map `0x80000000`, and a host pointer outside 16MB cannot survive that mask.

## What Changes

- Link `hmbtn-host` as large-address-aware, and map a 2MB PS1 RAM window at `0x80000000` plus the 1KB scratchpad at `0x1F800000` before any game code runs. A test on this machine (32-bit MinGW, 64-bit Windows) reserved and wrote both ranges only with the large-address-aware flag.
- Bind the overlay region symbols `overlay_low` and `overlay_high` to `0x800B7E20` and `0x8012DDE8` inside that window, as on the PS1.
- Keep the executable image at `0x00400000`, and require it to end below `0x01000000`.
- Give the host GPU layer one rule for 24-bit links: below `0x00200000` is an offset into the PS1 RAM window, `0x00400000` up to `0x01000000` is a host image address, and `0x00FFFFFF` ends the chain. `DrawOTag` and a host `ClearOTagR` use that rule.
- Do not edit `LINK_PRIM`, `ot_insert_prim`, or any file under `src/`.

## Capabilities

### New Capabilities

- `ps1-address-space`: The host maps the PlayStation RAM and scratchpad at their PS1 addresses, binds the overlay regions there, and keeps the executable image where a 24-bit link still names it.

### Modified Capabilities

- `gpu-packet-replay`: `DrawOTag` resolves 24-bit links into the PS1 window or the host image and stops at the `0x00FFFFFF` terminator. `ClearOTagR` builds chains in that form.

## Impact

- `cmake/host.cmake` link options (large-address-aware and the two overlay symbols), a new `src/pc_port/src/ps1_memory.c`, and `libgpu.c` / `libgpu.h`.
- `main_pc.c` maps memory after SDL and before the window loop. A mapping failure exits non-zero with a log line.
- Windows only for the mapping call; Linux would use `mmap` with a fixed address and is not verified here.
- Out of scope: calling `ps1_main`, VSync, CD data, and overlay data tables that hold absolute PS1 addresses (for example `family_tables` in `family.c` points at `0x80127F98`, which is overlay data the host keeps in its image, so that read sees the empty window).
