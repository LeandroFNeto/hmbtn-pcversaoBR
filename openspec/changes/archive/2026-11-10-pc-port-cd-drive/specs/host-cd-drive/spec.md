# Spec Delta

## Purpose

Serves the game's CD-ROM calls on the host from the files extracted from the original disc image, so the game reads the disc's real bytes at the positions it computes.

## ADDED Requirements

### Requirement: The disc folder comes from the original image
When the host build is given the original disc image, it SHALL extract the disc's files into a folder next to the host executable using the repository's disc extractor, preserving the disc's directory names.

#### Scenario: Build with the image
- **WHEN** the host configuration is built with the original disc image
- **THEN** the folder next to `hmbtn-host.exe` contains `A_FILE.BIN`, `A_FILE.HDT`, and `STR/01.XA`

#### Scenario: Build without the image
- **WHEN** the host configuration is built without a disc image
- **THEN** the host executable still builds

### Requirement: The drive refuses to start without the archive
Before the game runs, the host SHALL open the disc folder and SHALL log the missing file and exit non-zero if `A_FILE.HDT` or `A_FILE.BIN` is absent.

#### Scenario: Missing archive
- **WHEN** the host starts with the game enabled and the disc folder has no `A_FILE.BIN`
- **THEN** the process logs the missing file and exits non-zero without running the game

### Requirement: CdSearchFile finds disc names
`CdSearchFile` SHALL accept a disc name with a leading backslash, backslash separators, any letter case, and an optional `;N` version, and SHALL find the extracted file with that path. On success it SHALL fill the position with the file's first sector and the size with its length in bytes, and SHALL return the structure it was given. A name with no file SHALL return null.

#### Scenario: A_FILE.BIN
- **WHEN** the game calls `CdSearchFile` with `\A_FILE.BIN;1`
- **THEN** the returned size equals the length of the extracted `A_FILE.BIN`
- **AND** the returned position converts with `CdPosToInt` to that file's first sector

#### Scenario: Nested name
- **WHEN** the game calls `CdSearchFile` with `\STR\01.XA;1`
- **THEN** the call returns the structure it was given

#### Scenario: Unknown name
- **WHEN** the game calls `CdSearchFile` with a name no extracted file has
- **THEN** the call returns null

### Requirement: Positions convert like the drive
`CdIntToPos` and `CdPosToInt` SHALL convert between a sector number and a BCD minute-second-sector position that includes the 150-sector lead-in, and SHALL be inverses of each other.

#### Scenario: Round trip
- **WHEN** a sector number is converted with `CdIntToPos` and back with `CdPosToInt`
- **THEN** the result equals the original sector number

#### Scenario: First data sector
- **WHEN** sector 0 is converted with `CdIntToPos`
- **THEN** the position is minute `0x00`, second `0x02`, sector `0x00`

### Requirement: CdRead copies whole sectors from the seek position
After `CdControl(CdlSetloc, position)`, `CdRead(n, buffer, mode)` SHALL write `n` times 2048 bytes to `buffer`, taken from the file that holds each sector, starting at the seeked sector. Bytes past a file's end, or in a sector no file holds, SHALL be zero. The read SHALL advance the seek position by `n` sectors.

#### Scenario: Archive header bytes
- **WHEN** the game seeks to the first sector of `A_FILE.HDT` and reads one sector
- **THEN** the first bytes of the buffer equal the first bytes of the extracted `A_FILE.HDT`

#### Scenario: Member inside the archive
- **WHEN** the game seeks to `A_FILE.BIN`'s first sector plus `k` and reads one sector
- **THEN** the buffer equals bytes `k * 2048` to `k * 2048 + 2047` of `A_FILE.BIN`

#### Scenario: Past the end of a file
- **WHEN** a read covers a sector after the last byte of a file
- **THEN** the bytes of that sector in the buffer are zero

### Requirement: Reads complete before the caller polls
A read SHALL be finished when `CdRead` returns. `CdReadSync` SHALL then report done, `CdSync` SHALL report complete, and a callback registered with `CdReadCallback` SHALL be called once with the complete status.

#### Scenario: Completion after a read
- **WHEN** `CdRead` returns success
- **THEN** `CdReadSync(1, 0)` returns 0 and `CdSync(1, 0)` returns `CdlComplete`

#### Scenario: Read callback
- **WHEN** a callback is registered with `CdReadCallback` and `CdRead` runs
- **THEN** the callback is called once with `CdlComplete`

### Requirement: The game passes the title load
With the original disc extracted and the game enabled, the game SHALL load the title's sprite file without entering its `host_poll` error loop.

#### Scenario: Past vblank 41
- **WHEN** the host runs the game for ten seconds with the disc folder in place
- **THEN** the vblank counter keeps increasing past 41
- **AND** the window still responds and closing it exits with code 0
