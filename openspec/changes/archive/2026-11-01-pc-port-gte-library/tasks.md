# Tasks

## 1. The reference

- [x] 1.1 Build the image and dump the `LIBGTE` objects per design.md section 1 (the dump stays outside the repository). For each of the 22 functions, record here its object, the GTE operations it uses with their shift flag, the GTE registers it reads and writes, and anything that departs from the plain math. Also confirm:
  - the arctangent table at file offset 0x4D0C4 of `SLUS_011.15` equals `ratan.o`'s table;
  - the matrix stack depth and what overflow does;
  - each spec scenario value holds for the library's code, and correct the spec where one does not.

  Reading (2026-11-01, `docker build -t hmbtn-build .`, `tools/psyq_objects.py`, `mips-linux-gnu-objdump -d -r -EL`). Angles below are masked with `& 0xFFF` after taking the absolute value; a negative angle negates the sine. All products are 32-bit, `>> 12` is arithmetic, and results are stored as `s16` where the target is a matrix.
  - Tables, each the whole `.data` of its object, found at these `SLUS_011.15` file offsets: `rsin_tbl` (`sincos.o`, 0x48254, 1025 `s16`, `round(sin(i·π/2048)·4096)`), `rcossin_tbl` (`cstbl.o`, 0x490C4, 4096 × {sin, cos} `s16`, same rounding), `SQRT` (`sqrtbl.o`, 0x48A94, 192 `s16`, `floor(sqrt(64 + i)·512)`), the inverse root table (`msc02.o` `.data` + 20, 0x48C28, 192 `s16`, `floor(32768/sqrt(64 + i))`), and `ratan_tbl` (`ratan.o`, 0x4D0C4, 1025 `s16`, no closed formula). `ratan_tbl` equals the 0x4D0C4 block of the executable exactly.
  - `rsin` (`geo_00.o`): a ≥ 0 → `sin_1(a & 0xFFF)`, else `-sin_1(-a & 0xFFF)`. `sin_1(a)`: a ≤ 1024 → `t[a]`; ≤ 2048 → `t[2048 - a]`; ≤ 3072 → `-t[a - 2048]`; else `-t[4096 - a]`.
  - `rcos` (`geo_01.o`): a = |a| & 0xFFF; ≤ 1024 → `t[1024 - a]`; ≤ 2048 → `-t[a - 1024]`; ≤ 3072 → `-t[3072 - a]`; else `t[a - 3072]`.
  - `ratan2(y, x)` (`ratan.o`): absolute values, with the signs kept; x = y = 0 → 0. If y < x: v = (y & 0x7FE00000) ? y / (x >> 10) : (y << 10) / x, r = `t[v]`. Else: v = (x & 0x7FE00000) ? x / (y >> 10) : (x << 10) / y, r = 1024 - `t[v]`. Negative x → r = 2048 - r; negative y → r = -r.
  - `SquareRoot0` (`msc01.o`): GTE `LZCS`/`LZCR`; lzc = 32 → 0. e = lzc & ~1, s = (31 - e) >> 1, i = (e ≥ 24 ? a << (e - 24) : a >> (24 - e)) - 64, result = (`SQRT[i]` << s) >> 12, logical.
  - `csqrt` (`cor_05.o`, uses `Lzc` from `smp_00_1_325.o`, no table): a = 0 → 0. d = 8 - lzc(a); d ≥ 0 → k = d >> 1, x = a >> 2k; d < 0 → k = (d >> 1) + 1, x = a << -2k. Then r = `csqrt_1(x)` shifted left by k - 6 (right if negative). `csqrt_1`: X1 = x + 0x5D50AD, Y1 = x - 0x5D50AD; for i = 1..6, if Y ≥ 0 {X -= Y >> i; Y -= X_old >> i} else {X += Y >> i; Y += X_old >> i}, with step i = 4 done twice (hyperbolic CORDIC); returns X7.
  - `RotMatrix` (`fgo_01.o`): sx, cx, sy, cy, sz, cz from `rcossin_tbl`. m02 = sy; m12 = (-(cy·sx)) >> 12; m22 = (cy·cx) >> 12; m00 = (cz·cy) >> 12; m01 = (-(sz·cy)) >> 12; a = (cz·-sy) >> 12; m10 = ((sz·cx) >> 12) - ((a·sx) >> 12); m20 = ((sz·sx) >> 12) + ((a·cx) >> 12); b = (sz·-sy) >> 12; m11 = ((cz·cx) >> 12) + ((b·sx) >> 12); m21 = ((cz·sx) >> 12) - ((b·cx) >> 12). The translation is untouched.
  - `RotMatrixZXY` (`fgo_08.o`): m21 = sx; m20 = (-(cx·sy)) >> 12; m22 = (cx·cy) >> 12; m11 = (cz·cx) >> 12; m01 = (-(sz·cx)) >> 12; a = (sx·sy) >> 12; m00 = ((cz·cy) >> 12) - ((a·sz) >> 12); m10 = ((sz·cy) >> 12) + ((a·cz) >> 12); b = (sx·cy) >> 12; m02 = ((cz·sy) >> 12) + ((b·sz) >> 12); m12 = ((sz·sy) >> 12) - ((b·cz) >> 12).
  - `RotMatrixX/Y/Z` (`fgo_04/05/06.o`): multiply `m` on the left in place, all six inputs read first, one `>> 12` per element. X: row1 = (c·r1 - s·r2), row2 = (s·r1 + c·r2). Y: with n = -s, row0 = (c·r0 - n·r2), row2 = (n·r0 + c·r2). Z: row0 = (c·r0 - s·r1), row1 = (s·r0 + c·r1).
  - `TransMatrix` (`mtx_07.o`): copies v into m->t, returns m.
  - `PushMatrix`/`PopMatrix` (`mtx_006.o`): 20 entries of the 8 control words (rotation, translation); full → `printf("Error: Can't push matrix,stack(max 20) is full!\n")` and return; empty → `printf("Error: Can't pop matrix,stack is empty!\n")` and return.
  - `ApplyMatrixLV` (`mtx_004.o`): loads m as the GTE rotation (left there). Each component splits toward zero into hi = v / 32768 and lo = v % 32768. `MVMVA` R·IR with no translation, sf=0, on hi (IR as `s16`), then sf=1 on lo; result = MAC(lo) + (MAC(hi) << 3).
  - `RotTrans` (`smp_04.o`): `MVMVA` R·V0 + TR, sf=1, lm=0; stores MAC1 to MAC3 and FLAG.
  - `OuterProduct0` (`smp_00_1_324.o`): saves control words 0, 2, 4, writes v0's components there (the diagonal takes their low 16 bits), loads v1 into IR1 to IR3 (`s16`), `OP` sf=0, stores MAC1 to MAC3, restores the words.
  - `VectorNormal`, `VectorNormalS`, `VectorNormalSS` (`msc02.o`): IR = v (`s16`); `SQR` sf=0, lm=1; n = MAC1 + MAC2 + MAC3 (returned); e = lzc(n) & ~1, s = (31 - e) >> 1, IR0 = inverse table at the same index rule as `SquareRoot0`; IR = v; `GPF` sf=0; result = MAC >> s, arithmetic. `S` reads a `VECTOR` and writes `s16`.
  - `InitGeom` (`msc00.o`): `_patch_gte` (kernel handler, no host meaning), enables COP2, ZSF3 = 341, ZSF4 = 256, H = 1000, DQA = -4194, DQB = 0x1400000, OFX = OFY = 0. The matrix stack is left alone (spec corrected).
  - `SetFarColor` (`reg11.o`): RFC, GFC, BFC = r, g, b << 4. `SetFogNear` (`fog_01.o`): DQA = -(a·320) / h, DQB = 0x1400000; h = 0 traps on the PS1 (`break 7`).
  - Spec scenarios: all values hold by hand-tracing the code. Corrections: `InitGeom` (start values, stack untouched), `VectorNormal((3, 4, 0))` = (2457, 3276, 0) returning 25, and `ApplyMatrixLV`/`OuterProduct0`'s effect on the current rotation.

## 2. The implementation

- [x] 2.1 Extend `gte.c` per design.md section 3 (far color and depth-cue registers, `RT` and `OP` with the shift flag, leading-zero count, matrix stack). Verify with a small self-check: an `RT` with sf=0 keeps 32-bit results, an `OP` with sf=0 gives (0, 0, 1) for (1, 0, 0) × (0, 1, 0), and the leading-zero count of 1, 0x7FFFFFFF, and -1 matches the hardware (psx-spx, "GTE Arithmetic").
- [x] 2.2 Compute the quarter sine, `rcossin_tbl`, and square root tables at start, and load the arctangent table from the extracted disc when the CD drive opens (design.md section 2). Verify each computed table equals `SLUS_011.15` entry for entry, and the arctangent load passes its checks.

  The tables are computed on first use (also the inverse root table, found in 1.1), and the arctangent table is read through the host CD drive on the first `ratan2`. The self-check compares 9601 entries with the executable, all equal.
- [x] 2.3 Implement the 22 functions in `src/pc_port/src/libgte_math.c` from the task 1.1 notes, and remove their stubs from `libgte_stub.c`. Verify the host links with no duplicate symbol and no stub of those 22 left.

  Two inputs the PS1 does not survive get a safe host answer, logged once: `SquareRoot0` of a negative value (the PS1 reads outside its table) returns 0, and `SetFogNear` with h = 0 (the PS1 traps) leaves the depth cue as it was. `libgte_stub.c` keeps only the 7 functions no game code calls and the two matrix loads.
- [x] 2.4 Add the 10 missing prototypes to `libgte.h` and `-fno-builtin-csqrt` to `cmake/host.cmake`. Verify a clean build has no implicit-declaration warning for the 22 functions and no `csqrt` clash, and that the PS1 build is untouched (`git diff -- src/main src/overlay src/include` empty).

  Prototypes follow PSY-Q 4.7's `libgte.h` (`csqrt` is `int csqrt(int)`, `RotTrans` returns `void`, `VectorNormal*` return `long`). Clean build (`--clean-first`): no implicit declaration of the 22, no `csqrt` clash, no conflicting types; the diff under `src/` is empty.
- [x] 2.5 Add `src/pc_port/src/gte_library_check.c` to the `HMBTN_SELFTEST=1` chain per design.md section 5. Verify the log shows it passing, with the scenario values, the extra values from 1.1, and the number of table entries compared.

  33 checks pass. Extra values from the 1.1 reading, cross-checked with an independent Python transcription: `SquareRoot0(1000000)` = 999 and `(12345678)` = 3510; `csqrt` of 1.0, 4.0 (20.12) = 4096, 8192, of 1000000 = 64002; `ratan2(1, 2)` = 302, `(-300, -700)` = -1784; `RotMatrix(512, 256, 128)` = {3711, -739, 1567; 1651, 2623, -2676; -523, 3057, 2675}; `VectorNormalS(100, 200, 300)` = (1097, 2194, 3291). The new files compile with no warning under `-Wall -Wextra`.

## 3. The farm

- [x] 3.1 Run a new game into the farm (`build/host/keys.ps1`, `HMBTN_TRACE=1`, no memory card). Verify `field_probe_sweep` completes: no fault there, and the probe heights stay within a few hundred units of the ground's (log them once with `gdb` or a temporary trace). Record the next blocker here if the farm still stops, with its stack line.

  Run 1 (2026-11-01, under `gdb` with a breakpoint at the end of the sweep, `field.c:278`): the first sweep fills all 1125 cells of `farm_area.mode.quads` with a ground quad, and the last probe ends at height 0 (13800, 0, -8500), the flat field's height, instead of the 31000 of `pc-port-null-reads` run 2. Scene 13 → 2 at vblank 5706; the farm then ran to vblank 9204 (3498 vblanks) with no fault and no stall report, and the process exited normally on window close.

  Run 2 (same day, no debugger): scene 13 → 2 at vblank 5412, exit at vblank 7812 after 40 seconds on the farm (2400 vblanks), no fault, no stall. The farm draws (house, mailbox, player, weeds, stone, branch) and the mayor walks over and starts the first-morning dialogue ("Well, what do you think? This was your grandpa's farm."): `build/host/clean_farm.png`, `build/host/gte_farm_a.png`, `gte_farm_b.png`.

  Seen, not a blocker: a black rectangle left of the house in every capture. `MoveImage`/`MoveImage2` are implemented on the host, so it is not a missing VRAM copy stub; the cause is not investigated (recorded in the README).
- [x] 3.2 Update `README.md` per the README status rule: add the row for `pc-port-gte-library`, and update "A fazenda não carrega" to the next blocker or move it to "Já resolvido" if `pc-port-null-reads` 4.1 passes.

## Workflow follow-up

- Resume `pc-port-null-reads` task 4.1 (the 30-second farm run) after 3.1.
- Then `pc-port-memory-card` task 4.2 (save in the diary).
