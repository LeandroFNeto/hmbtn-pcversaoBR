# Design

## Context

See proposal.md for why. `ground_hit_primitive()` (`src/main/geometry/ground.c`) returns `ground_hit_prim_ptr`, which `ground_find_object` and `ground_listed_object_hit` reset to 0 and set only on a hit. Its readers:

| Place | Read | Null test |
| --- | --- | --- |
| `people/player.c:742` (`player_build_result`) | `*(s32 *)hit`, then 0x8000 when it has no high bits | no |
| `people/player.c:4380` | `*(u32 *)hit` into `player_status_word` | no |
| `farm_probe.c:920` (`farm_status_upper_bits`) | `*(u8 *)hit >> 7` | no |
| `farm_probe.c:928`, `:943` | the hit's flags | yes |
| `field/field.c:269`, `field/greenhouse.c:67`, `field/mine.c:174` (probe sweeps) | `hit->header & bit` | no |
| `field/greenhouse.c:113` | `hit->header` of a grid cell, null where the sweep found no ground | no |

On the PS1 the null reads land on the kernel's garbage area: word 0 is 3, so the high-bits test gives "no ground", bit 7 of byte 0 is 0, and bit 3 of the header is clear. `pc-port-null-reads` already seeds that RAM at 0x80000000 and provides `PS1_NULL_PTR`.

## Goals / Non-Goals

**Goals:** a new game gets past the flashback with the PS1's "no ground" outcome at each covered read.

**Non-Goals:** changing what the probe finds (a missing hit is normal here: the probe looks one tile ahead, past the edge of the ground models).

## Decisions

### 1. One patch per file, wrapping the read, not the getter

Wrapping `ground_hit_primitive()` itself in `ground.c` would also change the two readers that test for null (they would never see 0). So each unguarded read is wrapped where it happens:

- `player.c`: `ground_hit_primitive()` → `PS1_NULL_PTR(ground_hit_primitive())` (both occurrences are unguarded reads);
- `farm_probe.c`: only `*(u8 *)ground_hit_primitive()`;
- `field.c`: `hit->header & 8`; `greenhouse.c` and `mine.c`: `hit->header & FARM_QUAD_CELL_BIT` (in `greenhouse.c` this covers both the sweep and the grid-cell read).

Each patch also adds `#include <ps1_null.h>` after the file's `#include "types.h"`.

### 2. Verification

The run continues where `pc-port-memory-card` 4.2 stopped: past the flashback into free play. A later null access that is not a ground-hit read is recorded and stops the run.

## Risks / Trade-offs

- [`*(s32 *)` on the null hit reads the word the PS1 had right after boot] → The kernel's word 0 is 3 once the CD has been used (psx-spx); `ps1_memory_map` seeds that value.
