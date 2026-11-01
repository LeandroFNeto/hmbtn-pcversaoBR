# Spec Delta

## Purpose

Keeps the meaning of the game's C code the same on the PC host as on the PS1, where the PS1 compiler (GCC 2.95) accepted constructs that modern GCC treats differently, without editing the game sources.

## ADDED Requirements

### Requirement: Statically initialized trailing arrays keep their data
A struct ending in a zero-length array that the game initializes statically SHALL hold, on the host, every initializer element right after the struct's other members, as on the PS1. The struct's size and member offsets SHALL stay the same.

#### Scenario: Farm slot table
- **WHEN** the host reads the first offset of the farm's slot-six table of contents
- **THEN** it reads 0, and the second offset reads 1008

#### Scenario: Toy animation address
- **WHEN** the farm binds the first toy animation
- **THEN** the address it passes is `slotSixArt` itself, word-aligned, and the file there starts with the `.ANI` magic

### Requirement: Game sources stay unedited
Compatibility fixes SHALL be applied to copies under the build tree. The files under `src/main`, `src/overlay`, and `src/include` SHALL be unchanged, and the PS1 build SHALL keep compiling the originals.

#### Scenario: Diff after the fix
- **WHEN** the host executable has been built with the fix
- **THEN** `git diff -- src/main src/overlay src/include` is empty

#### Scenario: Patch target moved
- **WHEN** the text a host patch replaces is no longer found in its source file
- **THEN** configuring the host build fails and names the file and the text

### Requirement: Dropped initializers fail the build
The host build SHALL fail when compiling a game source produces the warning "excess elements in array initializer", and SHALL name the file and line.

#### Scenario: New occurrence
- **WHEN** a game source initializes a zero-length array that no host fix covers
- **THEN** the build stops with an error naming that source and line

### Requirement: The farm's tables are checked at start
With `HMBTN_SELFTEST=1`, the host SHALL check all 67 farm tables of contents (7 slot, 10 item, 50 pack). The first offset SHALL be 0 for slot and item tables and 4 × (`count` + 1) for pack tables, and the offset at `count` SHALL be greater than the first. A failure SHALL name the table and fail the self-test.

#### Scenario: Healthy tables
- **WHEN** the host starts with `HMBTN_SELFTEST=1`
- **THEN** the log shows the farm table check passing for all 67 tables

#### Scenario: Known values
- **WHEN** the check reads the slot-six table and the first pack table
- **THEN** they hold `0, 1008, 3072, 3688, 4236, 5376` and `12, 1008, 2200`

### Requirement: A new game reaches the farm
With the game running from the extracted disc, confirming the introduction's summary SHALL load the farm without the host stalling in the game's debug trap.

#### Scenario: First morning
- **WHEN** the player confirms "This is OK" after the name, birthday, farm, and dog questions
- **THEN** the farm scene draws and the frame counter keeps advancing for at least 30 seconds without a watchdog stall report
