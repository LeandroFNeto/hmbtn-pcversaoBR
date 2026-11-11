# Spec Delta

## Purpose

Gives the host process the PlayStation addresses the decompiled game stores through, and keeps every packet address the game can mask to 24 bits recoverable.

## ADDED Requirements

### Requirement: PS1 RAM is mapped at its PlayStation address
Before any game code runs, the host SHALL make `0x80000000` through `0x801FFFFF` readable and writable, initially zero.

#### Scenario: Work region write
- **WHEN** host startup has finished mapping memory
- **THEN** a byte written at `0x801BE1B0` reads back the same value
- **AND** a byte at `0x801F7000` that nothing wrote reads zero

### Requirement: The scratchpad is mapped
Before any game code runs, the host SHALL make `0x1F800000` through `0x1F8003FF` readable and writable.

#### Scenario: Scratchpad top word
- **WHEN** host startup has finished mapping memory
- **THEN** a word written at `0x1F8003FC` reads back the same value

### Requirement: A mapping failure stops startup
If either range cannot be mapped at its exact address, the host SHALL log which range failed and exit with a non-zero code before opening the game loop.

#### Scenario: Range already in use
- **WHEN** the PS1 RAM range cannot be mapped at `0x80000000`
- **THEN** the process logs the failed range and exits non-zero
- **AND** no game code runs

### Requirement: Overlay regions keep their PS1 addresses
The overlay region symbols SHALL resolve to `0x800B7E20` for the low region and `0x8012DDE8` for the high region.

#### Scenario: Region table
- **WHEN** host code reads the overlay region table entries
- **THEN** they are `0x800B7E20` and `0x8012DDE8`

### Requirement: The host image stays inside the 24-bit window
The executable image SHALL start at `0x00400000` and end below `0x01000000`, so masking a static address to 24 bits leaves it unchanged.

#### Scenario: Static packet address
- **WHEN** the address of any static object in the executable is masked with `0x00FFFFFF`
- **THEN** the result equals the original address
