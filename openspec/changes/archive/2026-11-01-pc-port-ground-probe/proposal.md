# Proposal

## Why

On the first morning, during the childhood flashback, the host faults in `player_build_result` (`people/player.c:742`): the probe of the tile ahead of the player finds no ground, `ground_hit_primitive()` returns null, and the code reads the hit's first word through it (`pc-port-memory-card` task 4.2, attempt 1). On the PS1 that read gives the kernel's word 3 at address 0, which has no high bits, and the code's own fallback turns it into "no ground" (0x8000). Two other functions in `farm_probe.c` test the hit for null; six places do not. Until this is covered, a new game cannot get past the flashback, and the diary save (`pc-port-memory-card` 4.2 and 4.3) cannot be reached.

## What Changes

- Host patches wrap the ground hit with `PS1_NULL_PTR` (from `pc-port-null-reads`) at the places that read it without a null test: `people/player.c` (2), `farm_probe.c` (1), `field/field.c` (1), `field/greenhouse.c` (2, the probe sweep and a grid cell read), and `field/mine.c` (1).
- The places that already test for null (`farm_probe.c` `farm_ground_kind` and the status probe) stay as they are.
- A new game is run past the flashback to the farm's free play.
- No change under `src/` outside `src/pc_port/`.

## Capabilities

### New Capabilities

- None.

### Modified Capabilities

- `host-source-compat`: adds that a ground probe that finds nothing reads, through the null hit, what the PS1 reads at address 0.

## Impact

- `cmake/host.cmake`: five more `hmbtn_host_patch` calls.
- `pc-port-memory-card` task 4.2 continues once the flashback passes.
- Out of scope: other null accesses found later (each recorded, and covered here only if it is the same ground-hit pattern).
