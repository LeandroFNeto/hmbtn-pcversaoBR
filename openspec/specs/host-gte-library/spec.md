# host-gte-library Specification

## Purpose
Gives the game's CPU-side `libgte` calls (trigonometry, square roots, rotation matrices, the matrix stack, vector transforms, normalization, and the fog settings) the same results on the PC host as the PS1 library, so 3D scenes compute the same geometry.

## Requirements

### Requirement: Results match the PS1 library
Every `libgte` function the game calls SHALL return, for every input, the value the PSY-Q 4.7 library linked into the game returns, including its rounding, wrap-around, and saturation. The values in this spec's scenarios are those of that library.

#### Scenario: No stub left in use
- **WHEN** the host is built
- **THEN** none of the 22 functions the game calls (`InitGeom`, `ratan2`, `csqrt`, `SquareRoot0`, `rsin`, `rcos`, `RotMatrix`, `RotMatrixX`, `RotMatrixY`, `RotMatrixZ`, `RotMatrixZXY`, `TransMatrix`, `PushMatrix`, `PopMatrix`, `ApplyMatrixLV`, `RotTrans`, `OuterProduct0`, `VectorNormal`, `VectorNormalS`, `VectorNormalSS`, `SetFarColor`, `SetFogNear`) is an empty body or a constant return

### Requirement: Angles use the PS1 tables
`rsin` and `rcos` SHALL take angles in units of 1/4096 of a turn, any integer including negatives, and return 1.12 fixed-point values from the same tables as the PS1. `ratan2(y, x)` SHALL return the angle of (x, y) in the same units.

#### Scenario: Sine and cosine
- **WHEN** the game calls `rsin` with 0, 512, 1024, 3072, -1024, and 4608, and `rcos` with 0 and 2048
- **THEN** they return 0, 2896, 4096, -4096, -4096, and 2896, and 4096 and -4096

#### Scenario: Arctangent quadrants
- **WHEN** the game calls `ratan2` with (y, x) = (0, 1), (1, 1), (1, 0), and (-1, 0)
- **THEN** it returns 0, 512, 1024, and -1024

### Requirement: Square roots match the PS1
`SquareRoot0` and `csqrt` SHALL return what the PS1 library returns, including where its table-based result differs from the exact root.

#### Scenario: Exact powers
- **WHEN** the game calls `SquareRoot0` with 0, 65536, and 1073741824
- **THEN** it returns 0, 256, and 32768

### Requirement: Rotation matrices
`RotMatrix`, `RotMatrixZXY`, `RotMatrixX`, `RotMatrixY`, and `RotMatrixZ` SHALL build or multiply 1.12 rotation matrices in the PS1 library's axis order and sign convention, and `TransMatrix` SHALL set a matrix's translation. None of them SHALL change the GTE's current matrix.

#### Scenario: Zero angles
- **WHEN** `RotMatrix` is called with angles (0, 0, 0)
- **THEN** the matrix is the identity (4096 on the diagonal, 0 elsewhere)

#### Scenario: Quarter turn about X
- **WHEN** `RotMatrixX(1024, m)` is applied to an identity `m`
- **THEN** `m` holds the library's quarter-turn about X, with entries of only 0, 4096, and -4096

### Requirement: The matrix stack
`PushMatrix` SHALL save the GTE's current rotation and translation, and `PopMatrix` SHALL restore the last saved pair, up to the PS1 library's depth of 20. A push past that depth or a pop of an empty stack SHALL leave the current matrix and the stack unchanged and log the library's message once.

#### Scenario: Nested use
- **WHEN** matrix A is set, pushed, matrix B is set, and the stack is popped
- **THEN** the current rotation and translation are A's

### Requirement: Vector transforms
`RotTrans` SHALL apply the GTE's current rotation and translation to a short vector and report the GTE flag, `ApplyMatrixLV` SHALL apply a given matrix's rotation to a 32-bit vector without losing its high bits and, as on the PS1, leave that matrix as the GTE's current rotation, and `OuterProduct0` SHALL return the cross product of two vectors unscaled, leaving the current rotation as it was.

#### Scenario: Translate
- **WHEN** the current rotation is identity, the translation is (10, 20, 30), and `RotTrans` is called with (1, 2, 3)
- **THEN** the result is (11, 22, 33) and the flag is 0

#### Scenario: Large vector
- **WHEN** `ApplyMatrixLV` is called with an identity matrix and (100000, -200000, 300000)
- **THEN** the result is (100000, -200000, 300000)

#### Scenario: Cross product
- **WHEN** `OuterProduct0` is called with (1, 0, 0) and (0, 1, 0)
- **THEN** the result is (0, 0, 1)

### Requirement: Normalization
`VectorNormal`, `VectorNormalS`, and `VectorNormalSS` SHALL scale a vector to length 4096 (1.12) with the PS1 library's result.

#### Scenario: Three-four-five
- **WHEN** `VectorNormal` is called with (3, 4, 0)
- **THEN** the result is (2457, 3276, 0), and the function returns 25 (the squared length)

### Requirement: Fog and geometry settings are kept
`SetFarColor` and `SetFogNear` SHALL store their values in the host GTE's far color and depth-cue registers as the PS1 library computes them, and `InitGeom` SHALL set the host GTE's settings to the PS1 library's start state, leaving the matrix stack as it is.

#### Scenario: After InitGeom
- **WHEN** `InitGeom` has run
- **THEN** the screen distance is 1000, the screen offset is (0, 0), DQA is -4194, DQB is 0x1400000, and the average-Z factors are 341 and 256

#### Scenario: Fog near
- **WHEN** `SetFogNear(a, h)` is called with h not 0
- **THEN** DQA is -(a × 320) / h, truncated toward zero, and DQB is 0x1400000

### Requirement: Tables are checked against the game
With `HMBTN_SELFTEST=1` and the disc extracted, the host SHALL check that its sine, cosine, square root, and inverse square root tables equal the game's executable byte for byte, that the arctangent table read from it starts at 0 and reaches 512, and that every scenario value of this spec holds.

#### Scenario: Self-check
- **WHEN** the host starts with `HMBTN_SELFTEST=1` and the disc extracted
- **THEN** the log shows the GTE library check passing, with the number of table entries compared

### Requirement: The game sources compile against complete declarations
The game's calls to these functions SHALL compile against prototypes, without implicit-declaration warnings for them and without the `csqrt` built-in clash.

#### Scenario: Clean compile
- **WHEN** the host is built from clean
- **THEN** no warning names an implicit declaration of one of the 22 functions, nor a conflict with the built-in `csqrt`

### Requirement: The farm finds its ground
With the game running from the extracted disc, the farm's first frame SHALL settle its field probes on the ground, so the field grid fills as on the PS1.

#### Scenario: Field sweep
- **WHEN** a new game enters the farm
- **THEN** `field_probe_sweep` completes without a fault, and the probe heights stay within the ground's height range instead of jumping to unrelated values

### Requirement: Back color and light matrices are kept
`SetBackColor` SHALL store its three components, and `SetColorMatrix` and `SetLightMatrix` SHALL store the nine entries of the matrix they are given. A later read SHALL return those stored values. Emitting a polygon packet SHALL NOT replace that packet's corner colors with a color computed from these registers.

#### Scenario: Values round-trip
- **WHEN** `SetBackColor(10, 20, 30)` is called and each matrix setter is called with its own nine entries
- **THEN** the back color reads (10, 20, 30) and each matrix reads back as written

#### Scenario: Packet colors win
- **WHEN** a gouraud packet with known corner colors is drawn after those registers were set to different colors
- **THEN** the drawn corners use the packet's colors
