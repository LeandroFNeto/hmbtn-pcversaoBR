# Spec Delta

## Purpose

Gives the decompiled sources a host-side PSY-Q surface so every SDK call and type they already use compiles and links, without driving PlayStation hardware.

## ADDED Requirements

### Requirement: Included SDK headers resolve
On the host configuration, every PSY-Q header the current sources include SHALL resolve to a host declaration. That set is the headers included today: `libgpu.h`, `libgte.h`, `libgs.h`, `libetc.h`, `libcd.h`, `libapi.h`, `libpad.h`, `libsnd.h`, `libspu.h`, `libmcrd.h`, `libmcx.h`, `libmath.h`, `kernel.h`, `abs.h`, `memory.h`, `inline_c.h`, `gtemac.h`, and the SDK `sys/types.h` types those headers rely on.

#### Scenario: GPU header included by game code
- **WHEN** a gameplay translation unit includes `libgpu.h` in the host configuration
- **THEN** the compiler finds a host header
- **AND** the types and macros that translation unit uses from that header are declared

#### Scenario: Header outside the included set
- **WHEN** a source file includes a PSY-Q header that no current source includes
- **THEN** this capability does not require that header to exist

### Requirement: Referenced symbols link
Every PSY-Q function and data symbol referenced by the current sources SHALL have one host definition that the host executable can link.

#### Scenario: Unresolved SDK call
- **WHEN** the host executable is linked
- **THEN** the link does not fail on an undefined PSY-Q symbol referenced by the current sources

#### Scenario: Symbol the sources do not reference
- **WHEN** a PSY-Q symbol exists in the real SDK but no current source references it
- **THEN** this capability does not require a host definition for that symbol

### Requirement: Stubs do not touch hardware
A host stub SHALL return without accessing GPU, GTE, CD-ROM, SPU, or memory-card hardware, and without opening a display or reading a game disc.

#### Scenario: Graphics call during a host build
- **WHEN** the host executable has been linked
- **THEN** the GPU, GTE, and graphics-library definitions it contains do not open a window and do not submit a draw command to a device

#### Scenario: CD call during a host build
- **WHEN** the host executable has been linked
- **THEN** the CD definitions it contains do not read a disc or a `/mods` directory

### Requirement: Host objects contain no MIPS GTE opcodes
The host configuration SHALL compile the translation units that include the project GTE command header without emitting MIPS coprocessor encodings.

#### Scenario: Geometry units compile on the host
- **WHEN** the host configuration compiles the model and vector-length translation units
- **THEN** compilation succeeds with a modern GCC or Clang
- **AND** those host objects do not contain MIPS `.word` GTE opcodes
