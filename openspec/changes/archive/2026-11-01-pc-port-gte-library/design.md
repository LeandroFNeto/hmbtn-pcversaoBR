# Design

## Context

See proposal.md for why. What the investigation found (2026-11-01):

- `src/pc_port/src/libgte_stub.c` holds 29 functions that do nothing or return 0. The game calls 22 of them. The call sites:

  | Function | Calls | Files |
  | --- | --- | --- |
  | `SquareRoot0` | 10 | `math_util.c`, `vec_length.c`, `event_commands.c`, `mg2.c`, `mg3.c`, `mg4.c` |
  | `PushMatrix`, `PopMatrix` | 9 each | `model.c`, `player.c`, `scene.c`, `scene_view.c`, `mg2.c`, `mg3.c`, `mg5.c` |
  | `ratan2` | 6 | `camera.c`, `pad.c`, `villager.c`, `mg2.c`, `mg4.c` |
  | `RotMatrix`, `RotTrans` | 4 each | `scene_view.c`, `mg2.c`, `mg3.c`, `mg5.c` |
  | `RotMatrixX`, `RotMatrixY`, `VectorNormal`, `VectorNormalS` | 3 each | `model.c` (and `math_util.c`) |
  | `rsin`, `rcos`, `VectorNormalSS` | 2 each | `math_util.c`, `villager.c`, `model.c` |
  | the other 9 | 1 each | `main.c`, `pad.c`, `model.c`, `scene.c`, `ground.c` |

- `libgte.h` (host) does not declare `RotMatrixZXY`, `PushMatrix`, `PopMatrix`, `RotTrans`, `OuterProduct0`, `VectorNormal`, `VectorNormalS`, `VectorNormalSS`, `SetFarColor`, or `SetFogNear`; the game calls them implicitly (the `-Wimplicit-function-declaration` warnings in `scene.c` and others). Its `long csqrt(long)` clashes with GCC's complex `csqrt` built-in, a warning in every file that includes it.
- The host GTE (`gte.c`) keeps the rotation, translation, offset, and screen distance, the projection, and a few ops (`rtps`, `rtpt`, `rtv0`, `rtir`, `op12`, `sqr0`, `nclip`, `avsz3/4`). It has no matrix stack, no `RT` with full 32-bit results, no `OP` without the shift, no leading-zero count, and no far color or depth-cue registers.
- The PS1 build links PSY-Q 4.7 `LIBGTE.LIB` objects (`sdk/objects.txt`), among them `SINCOS`, `CSTBL`, `SQRTBL`, `RATAN`, `MSC00` to `MSC02`, `MTX_004` to `MTX_12`, `FOG_01`, `GEO_00`, `GEO_01`. The build image has PSY-Q 4.7 and `binutils-mips-linux-gnu`. `tools/psyq_objects.py` converts exactly those objects to ELF, so `mips-linux-gnu-objdump -d -r` shows each function's code with its relocations.
- The tables in the game's `SLUS_011.15` (file offsets in the extracted disc):
  - 0x48254, 1025 `s16`: equal to `round(sin(i·π/2048)·4096)` for every entry (the quarter sine table);
  - 0x490C4, 4096 pairs of `s16`: equal to `round(sin(i·π/2048)·4096)`, `round(cos(i·π/2048)·4096)` for every entry (`rcossin_tbl`);
  - 0x48A94, 192 `s16`: equal to `floor(sqrt(64 + i)·512)` for every entry (the square root table);
  - 0x4D0C4, 1025 `s16`, increasing from 0 to 512: the likely arctangent table. No closed formula found; the best fit, `floor(atan((i + 1)/1023)·2048/π)`, matches 966 of 1025.

## Goals / Non-Goals

**Goals:**

- Bit-exact results for the 22 functions.
- No Sony code or data in the repository.

**Non-Goals:**

- The 7 stubs no game code calls.
- The GTE's lighting and depth-cue operations (`ncs`, `ncds`, `dpcs`, and the rest). This change only stores the settings they read.
- Speed: these run a few hundred times a frame at most.

## Decisions

### 1. The reference is the library's own code

For each function, read its object in the build image:

```sh
docker run --rm -v "$PWD":/src:ro -v /tmp/gte:/out hmbtn-build bash -c \
  'python3 tools/psyq_objects.py --sdk /opt/psyq/sdk-4.7 --objects sdk/objects.txt \
     --patches sdk/patches.txt --out /out \
   && mips-linux-gnu-objdump -d -r -EL /out/libgte/*.o > /out/libgte.txt'
```

The host implementation is written in C from what that code does: the GTE operation and its shift flag, the order of multiplications, where it shifts, where it clamps, how it handles signs. No instructions or data are copied. For each function, tasks.md records its object, the GTE operations it uses, and anything surprising (a clamp, a different rounding). The scenario values in the spec are checked against that reading before the self-check is written. If one differs, the spec scenario is corrected, not the code.

Alternative considered: **implementing from the PSY-Q manual's formulas.** The manual gives the math, not the rounding and operation order, so results would drift by one in places. That breaks the "same geometry as the PS1" goal and makes later comparisons with an emulator noisy.

### 2. Tables

- The quarter sine table, `rcossin_tbl`, the square root table, and the inverse square root table of `VectorNormal` (`msc02.o`, 192 `s16` at file offset 0x48C28, equal to `floor(32768/sqrt(64 + i))`, found in task 1.1) are computed at host start from their formulas. They match the game's tables entry for entry, and computing them keeps Sony's data out of the repository.
- The arctangent table is read from the extracted disc's `SLUS_011.15`, 1025 `s16` at file offset 0x4D0C4, through the host CD drive the first time `ratan2` runs (the drive is open by then). Task 1.1 confirms this is `RATAN`'s table by comparing it with the `ratan.o` data in the image. The load checks that it starts at 0, never decreases, and ends at 512. If the disc is missing or the check fails, `ratan2` logs once and returns 0. The game needs the disc to run at all, and `HMBTN_RUN_GAME=ON` already refuses to start without it.
- The self-check compares the computed tables with the executable, so a formula that is off fails loudly.

Alternative considered: **reading every table from the executable.** That works, but it hides whether the formulas hold. Computing the three that have exact formulas also keeps the host's math working without a disc.

### 3. GTE state and the matrix stack

`gte.c` gains:

- the far color (`RFC`, `GFC`, `BFC`) and depth-cue (`DQA`, `DQB`) registers, set by `SetFarColor` and `SetFogNear` the way the library computes them, and set to the library's start values by `InitGeom`;
- `MVMVA` (rotation, with V0 or IR as the vector, with or without translation), `OP`, `SQR`, and `GPF`, each with its shift flag, plus the leading-zero count, with MAC1 to MAC3 as 32-bit (44-bit internally, checked with 64-bit math), IR1 to IR3 saturated, and FLAG bits, for the functions that use them;
- a matrix stack of the library's depth (20 in PSY-Q 4.7, confirmed from `mtx_*`/`reg*` in task 1.1) holding rotation and translation. Overflow and underflow log once and leave the current matrix, matching what the library does on the console (it prints an error and returns).

The functions live in a new `src/pc_port/src/libgte_math.c`. The empty stubs for those 22 functions are removed from `libgte_stub.c`, which keeps only the 7 unused ones.

### 4. Declarations and the `csqrt` warning

`libgte.h` gets the 10 missing prototypes with the PSY-Q 4.7 signatures (from `/opt/psyq/include/libgte.h` in the image). `cmake/host.cmake` adds `-fno-builtin-csqrt`, so GCC no longer treats `csqrt` as the complex built-in. The library's `csqrt` takes and returns `long`, and the game calls it that way (`pad.c`).

### 5. Checks

- `src/pc_port/src/gte_library_check.c`, in the `HMBTN_SELFTEST=1` chain: every scenario value of the spec, plus at least one value per function taken from the task 1.1 reading (for example a non-square for `SquareRoot0` and an off-axis angle for `ratan2`), and the table comparison.
- The farm run continues in `pc-port-null-reads` task 4.1. This change only requires that the field sweep passes (spec, "The farm finds its ground").

## Risks / Trade-offs

- [The arctangent offset is wrong in another release of the game] → The host is built for `SLUS_011.15`, the release the decomp targets. The load check (0 to 512, increasing) fails loudly on anything else.
- [A function reads GTE state the host does not model yet] → Task 1.1 lists each function's GTE reads and writes. Anything outside Decision 3 is added to it or recorded as a gap before coding.
- [Reading the library's code is slow] → 22 small functions in about 12 objects; most are a dozen instructions around one GTE operation.
- [Docker is needed for the reference] → Only while implementing. The host build itself does not need it.

## Migration Plan

1. The reference reading and the scenario check (task 1.1).
2. GTE state, tables, and the functions.
3. Declarations, the warning flag, and the self-check.
4. The farm's field sweep; then `pc-port-null-reads` 4.1 resumes.
5. Roll back by restoring `libgte_stub.c`; nothing else depends on the new file.

## Open Questions

None. (`InitGeom` sets the projection defaults, screen distance 1000 and offset 0, and leaves the matrix stack alone; task 1.1.)
