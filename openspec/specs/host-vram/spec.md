# host-vram Specification

## Purpose
Keeps the PlayStation's 1024 × 512 VRAM on the host and serves every transfer the game makes into it, out of it, and inside it, so textures and palettes are where the GPU commands expect them.

## Requirements

### Requirement: VRAM is 1024 by 512 sixteen-bit words
The host SHALL hold a VRAM of 1024 × 512 16-bit words, zero at startup, addressed by x from 0 to 1023 and y from 0 to 511, storing each word exactly as the game wrote it.

#### Scenario: Initial content
- **WHEN** no transfer has written VRAM
- **THEN** every word reads zero

### Requirement: LoadImage writes a rectangle
`LoadImage(rect, p)` SHALL copy `rect->w × rect->h` 16-bit words from `p`, row by row, into VRAM at (`rect->x`, `rect->y`). Coordinates SHALL wrap at 1024 horizontally and 512 vertically, as on the PS1.

#### Scenario: Round trip
- **WHEN** a 16 × 2 rectangle is loaded at (640, 0) and the same rectangle is stored back with `StoreImage`
- **THEN** the stored words equal the loaded words

#### Scenario: CLUT row
- **WHEN** the game loads a 16 × 1 rectangle at a CLUT position
- **THEN** a textured draw using that CLUT position reads those 16 colors

### Requirement: StoreImage reads a rectangle
`StoreImage(rect, p)` SHALL copy `rect->w × rect->h` words from VRAM at (`rect->x`, `rect->y`) into `p`, row by row.

#### Scenario: Palette read-back
- **WHEN** `clut.c` stores a 16-color row it loaded earlier
- **THEN** the buffer holds the loaded colors

### Requirement: MoveImage copies inside VRAM
`MoveImage(rect, x, y)` SHALL copy the VRAM rectangle at `rect` to (`x`, `y`), reading the whole source before writing, so overlapping copies give the source's original words.

#### Scenario: Texture animation slot
- **WHEN** an 8 × 8 block is moved from (512, 0) to (520, 0)
- **THEN** the destination holds the block and the source columns that do not overlap the destination are unchanged

### Requirement: ClearImage fills a rectangle
`ClearImage(rect, r, g, b)` and `ClearImage2` SHALL fill the VRAM rectangle with the 15-bit color made of the top five bits of `r`, `g`, and `b`. When the rectangle covers the displayed area, the host SHALL also clear the OpenGL frame to that color.

#### Scenario: Screen clear
- **WHEN** the game clears (0, 0, 320, 480) to black
- **THEN** those VRAM words read `0x0000`
- **AND** the next presented frame starts from black

### Requirement: Ordering-table transfers run in order
While walking an ordering table, `DrawOTag` SHALL perform a `DR_LOAD` packet as a `LoadImage` of its rectangle and data, and a `DR_MOVE` packet as a `MoveImage`, at their position in the chain, so packets after them sample the new words.

#### Scenario: CLUT fade in the chain
- **WHEN** a chain holds a `DR_LOAD` of a 16 × 1 CLUT row followed by a sprite that uses that CLUT
- **THEN** the sprite is drawn with the colors from the `DR_LOAD`

### Requirement: Every VRAM write passes one observer
Every write into VRAM, from `LoadImage`, `ClearImage`, `MoveImage`, `DR_LOAD`, or `DR_MOVE`, SHALL be reported with its rectangle and source to one observer function before it lands. The default observer SHALL do nothing.

#### Scenario: Upload reported
- **WHEN** the game loads an image with `LoadImage`
- **THEN** the observer is called once with that rectangle and data pointer

### Requirement: VRAM uses no memory per frame
The VRAM storage and its OpenGL texture SHALL be created once, SHALL NOT be allocated or freed per transfer or per frame, and SHALL be released when the host exits.

#### Scenario: Long run
- **WHEN** the game runs for one minute
- **THEN** the process's private memory at the end is within 1 MB of its value after the first ten seconds
