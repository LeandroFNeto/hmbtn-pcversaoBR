# Spec Delta

## Purpose

Copies the picture the host just drew back into VRAM when the game reads a rectangle that overlaps the draw buffer, so a later read of that rectangle returns those pixels.

## ADDED Requirements

### Requirement: A draw-buffer read returns the drawn picture
When `StoreImage` overlaps the draw buffer, each overlapping word SHALL be the pixel drawn there, packed as a PS1 15-bit color, and SHALL be written into VRAM before the call returns. The draw buffer is the 320 by 240 region the frame was drawn into, at that buffer's origin in VRAM. A word of the rectangle outside the overlap SHALL stay the word already in VRAM.

#### Scenario: Drawn block
- **WHEN** the frame has drawn a flat rectangle of a known color and `StoreImage` reads an 8 by 8 block inside it
- **THEN** every stored word is that color in 15-bit, and reading the same block again without drawing returns those words

#### Scenario: Below the first buffer
- **WHEN** the frame was drawn into the buffer whose origin is (0, 240) and `StoreImage` reads (0, 240, 320, 240)
- **THEN** the words are the pixels of that frame, not the words that were in VRAM before the frame

### Requirement: A read outside the draw buffer is unchanged
`StoreImage` of a rectangle that does not overlap the draw buffer SHALL return the words already in VRAM and SHALL NOT replace them with pixels from the frame.

#### Scenario: CLUT row
- **WHEN** the game stores a 16 by 1 CLUT row that lies outside the draw buffer
- **THEN** the buffer holds the colors last loaded into that row

#### Scenario: Photo scratch
- **WHEN** the game stores the rectangle (896, 256, 64, 256)
- **THEN** the words are the ones already stored there, not a copy of the 320 by 240 picture

### Requirement: Reading the picture does not allocate
Refreshing and copying a draw-buffer rectangle SHALL use only storage created with the host. It SHALL NOT allocate or free memory per call or per frame.

#### Scenario: Many frames
- **WHEN** the game stores a draw-buffer rectangle on successive frames
- **THEN** those reads add no allocation
