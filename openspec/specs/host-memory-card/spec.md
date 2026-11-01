# host-memory-card Specification

## Purpose

Gives the PC host a persistent PS1 memory card in slot 1, stored as a raw 128 KB card image, so the game's own save, load, and memory card screens work unchanged and saves survive closing the host.

## Requirements

### Requirement: The card is a raw 128 KB image file
Slot 1 SHALL be backed by one file of exactly 131,072 bytes in the raw PS1 memory card layout: 16 blocks of 8,192 bytes, each of 64 frames of 128 bytes, with block 0 holding the header and directory and blocks 1–15 holding file data. The file SHALL be `mcd001.mcr` in the executable's folder unless `HMBTN_MEMCARD` names another path.

#### Scenario: Default location
- **WHEN** the host starts without `HMBTN_MEMCARD`
- **THEN** the card in use is `mcd001.mcr` next to the executable, and it is 131,072 bytes long

#### Scenario: Path override
- **WHEN** the host starts with `HMBTN_MEMCARD` set to a file path
- **THEN** the card in use is that file and `mcd001.mcr` is not created

### Requirement: A missing card is created formatted
When the card file does not exist at start, the host SHALL create it as a formatted, empty card: the `MC` header frame, 15 free directory frames, the empty broken-sector list, and the header copy in the last frame of block 0, every frame with its PS1 XOR checksum. If the file cannot be created, slot 1 SHALL report no card and the host SHALL keep running.

#### Scenario: First run
- **WHEN** the host starts and the card file does not exist
- **THEN** the file is created with 131,072 bytes, starts with `MC`, and the game sees a formatted card with 15 free blocks

#### Scenario: Read-only location
- **WHEN** the card file cannot be created
- **THEN** the log names the path and the error, and the game's save screen reports no memory card in slot 1

### Requirement: The card answers the game's probes with PS1 states
`MemCardExist` and `MemCardAccept` on slot 1 SHALL report no error for a formatted card, "not formatted" for an image without the `MC` header, and "invalid" for a file of the wrong size or one that cannot be read. Slot 2 SHALL report no card. The host SHALL never report a new card, because the card cannot be swapped while running.

#### Scenario: Formatted card
- **WHEN** the save screen probes slot 1 with a formatted image
- **THEN** it reaches the slot list instead of a no-card, damaged, or format message

#### Scenario: Truncated file
- **WHEN** the card file is shorter than 131,072 bytes
- **THEN** the probe reports a damaged card and the file is not overwritten

#### Scenario: Empty slot 2
- **WHEN** the game probes slot 2
- **THEN** it reports no card in that slot

### Requirement: Commands finish at once and report once
Every card command SHALL complete before its call returns. `MemCardSync`, in wait or poll mode, SHALL return 1 with the command code (Exist 1, Accept 2, ReadFile 3, WriteFile 4, ReadData 5, WriteData 6) and its result the first time after a command, and −1 when no result is waiting. It SHALL never report busy. A registered `MemCardCallback` SHALL receive the same pair.

#### Scenario: Write then poll
- **WHEN** the game issues a file write and calls `MemCardSync(1, …)` twice
- **THEN** the first call returns 1 with command 4 and result 0, and the second returns −1

#### Scenario: No command pending
- **WHEN** the game calls `MemCardSync` with no command issued since the last result
- **THEN** it returns −1

### Requirement: The directory lists the card's files
`MemCardGetDirentry` SHALL return no error and fill one entry per matching file, up to the requested maximum, with its name, its size in bytes (blocks × 8,192), and its first block, and SHALL store the number of entries filled. A name SHALL match exactly; `*` SHALL match every file.

#### Scenario: Free space count
- **WHEN** the card holds one 4-block save and the game lists `*`
- **THEN** one entry is returned with size 32,768, so the game counts 11 free blocks

#### Scenario: Missing save
- **WHEN** the game lists a save name that is not on the card
- **THEN** the call returns no error with 0 entries

### Requirement: Files are created and deleted in directory blocks
`MemCardCreateFile(chan, name, blocks)` SHALL take the lowest free blocks, chain them in the directory with the name and size, and write the directory to the file. It SHALL report "already exists" for a name in use and "block full" when fewer free blocks remain. `MemCardDeleteFile` SHALL free a file's blocks or report the file missing.

#### Scenario: New save file
- **WHEN** the game creates `BASLUS-01115-FARM0` with 4 blocks on an empty card
- **THEN** the call returns no error and the directory shows that name over blocks 1 to 4

#### Scenario: Card full
- **WHEN** the game creates a 4-block file with 3 free blocks
- **THEN** the call reports block full and the directory is unchanged

### Requirement: File reads and writes reach the image
`MemCardReadFile` and `MemCardWriteFile` SHALL copy between the game's buffer and the file's data at the given byte offset, both multiples of 128 within the file's size. A write SHALL be on disk before its result is reported. A missing file SHALL give "file not exist"; a bad offset or length SHALL give "invalid".

#### Scenario: Round trip
- **WHEN** the game writes 32,768 bytes to a save file, the host restarts, and the game reads them back
- **THEN** the bytes read equal the bytes written

#### Scenario: Header read
- **WHEN** the game reads 128 bytes at offset 0x200 of an existing save
- **THEN** it receives the save's header frame from the image

### Requirement: The card can be formatted and unformatted
`MemCardFormat` SHALL rewrite block 0 as an empty formatted card and leave the data blocks as they are. `MemCardUnformat` SHALL clear the header so the card reports "not formatted". Both SHALL write the file before returning.

#### Scenario: Format from the game
- **WHEN** the card reports not formatted and the player accepts formatting on the save screen
- **THEN** the card is formatted and the save continues

### Requirement: The card is a plain memory card, not a PocketStation
After the game asks for the card type (`McxCardType`) on a slot holding the card, `McxSync` SHALL report that the device does not answer PocketStation commands (`McxErrInvalid`). Other PocketStation commands, and slot 2, SHALL report no device.

#### Scenario: Memory card screen
- **WHEN** the memory card screen probes slot 1
- **THEN** it shows a memory card, not a PocketStation

### Requirement: The image works in emulators
The image SHALL keep the PS1 directory format: state codes for free, first, middle, and last blocks, the next-block link, the file size in the first entry, the name, and the XOR checksum in byte 127 of every directory frame.

#### Scenario: Emulator import
- **WHEN** an image with a save from the host is opened in a PS1 emulator's memory card manager
- **THEN** the save is listed under its name with 4 blocks

### Requirement: Saving and loading work in the game
With the game running from the extracted disc, a save made from the diary SHALL survive closing the host, and CONTINUE on the title SHALL load it. No card call SHALL block the frame loop.

#### Scenario: Save, close, continue
- **WHEN** the player saves in the diary to slot 1, closes the host, starts it again, and chooses CONTINUE
- **THEN** the load screen lists the save, and loading it returns to the farm with the saved name and date

#### Scenario: Frames keep running
- **WHEN** the save or load screen is probing or writing the card
- **THEN** the host keeps presenting frames and the window stays responsive
