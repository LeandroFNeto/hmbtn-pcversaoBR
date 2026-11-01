# Proposal

## Why

The farm's first frame faults in `field_probe_sweep` (`pc-port-null-reads` task 4.1, run 2): `ground_height_at` builds the ground plane's normal with `OuterProduct0`, which on the host is an empty stub, so the height comes out as stack garbage (31000), the next probe misses the ground, and the code reads through the null hit. `OuterProduct0` is one of 22 `libgte` functions the game calls that the host leaves empty (`src/pc_port/src/libgte_stub.c`): sines and cosines, arctangent, square roots, rotation matrices, the matrix stack, `RotTrans`, `ApplyMatrixLV`, vector normalization, and the fog settings. Every 3D scene (the farm, the minigames, the models) depends on them, so fixing them one by one as each frame breaks would stall the port at every new scene.

## What Changes

- The 22 `libgte` functions the game calls get host implementations that return what the PS1 library returns, bit for bit:
  - `rsin`, `rcos`, `ratan2`, `SquareRoot0`, `csqrt`;
  - `RotMatrix`, `RotMatrixX`, `RotMatrixY`, `RotMatrixZ`, `RotMatrixZXY`, `TransMatrix`;
  - `PushMatrix`, `PopMatrix`, `RotTrans`, `ApplyMatrixLV`, `OuterProduct0`;
  - `VectorNormal`, `VectorNormalS`, `VectorNormalSS`;
  - `InitGeom`, `SetFarColor`, `SetFogNear`.
- The reference for each is the code of the PSY-Q 4.7 `LIBGTE` object the game links, read in the build image (the repository's `tools/psyq_objects.py` and the image's MIPS `objdump`). Nothing of Sony's goes into the repository.
- The lookup tables: the sine, cosine, and square root tables are computed on the host from closed formulas that reproduce the game's tables exactly. The arctangent table has no such formula, so it is read from the game's own executable on the extracted disc.
- The host GTE (`gte.c`) gains the state these functions need: the matrix stack, the full-precision `RT` and `OP` results, leading-zero count, and the far color and fog registers.
- `libgte.h` declares the functions the game now calls undeclared, and the `csqrt` built-in clash warning goes away.
- A self-check (`HMBTN_SELFTEST=1`) tests every function against known values and the tables against the disc.
- No change under `src/` outside `src/pc_port/`.

## Capabilities

### New Capabilities

- `host-gte-library`: The `libgte` math the game calls on the CPU side (trigonometry, roots, matrices, the matrix stack, vector transforms, normalization, fog settings), with the PS1 library's results.

### Modified Capabilities

- None. `gte-screen-transform` (the projection) keeps its requirements.

## Impact

- `src/pc_port/src/libgte_stub.c` (replaced by real implementations), `src/pc_port/src/gte.c`, `src/pc_port/include/libgte.h`, a new `gte_library_check.c`, and its hook in `main_pc.c`.
- `cmake/host.cmake`: `-fno-builtin-csqrt`.
- The extracted disc's `SLUS_011.15` becomes a read-only input of the host (the arctangent table, and the table cross-check).
- `pc-port-null-reads` task 4.1 (the farm run) continues once the farm's ground works; `pc-port-memory-card` tasks 4.2 and 4.3 wait on it.
- Out of scope: the 7 stubs no game code calls (`SquareRoot12`, `ScaleMatrix`, `MulMatrix`, `MulMatrix0`, `CompMatrix`, `ApplyMatrix`, `RotTransPers`), and the GTE's lighting and depth-cue operations, which use the fog settings but are not part of `libgte`'s CPU-side calls.
