# Spec Delta

## ADDED Requirements

### Requirement: A copy into the album writes the drawn pixels
When a copy inside VRAM has a source that overlaps the draw buffer and a destination that overlaps `(896, 256, 64, 256)`, each destination word whose source lies in that overlap SHALL be the pixel drawn there, packed as a PS1 15-bit color, before the copy returns. The draw buffer is the 320 by 240 region the frame was drawn into. A destination word whose source lies outside that overlap SHALL be the word already at the source.

#### Scenario: Block inside the frame
- **WHEN** the frame has drawn a flat rectangle of a known color and a copy moves an 8 by 8 block inside it to `(896, 256)`
- **THEN** every word of that block at the destination is that color in 15-bit

#### Scenario: Below the first buffer
- **WHEN** the frame was drawn into the buffer whose origin is `(0, 240)` and a copy moves an 8 by 8 block inside it to `(896, 256)`
- **THEN** the destination words are the pixels of that frame

#### Scenario: Ordering-table copy
- **WHEN** the same 8 by 8 copy is ordering-table command `0x80`
- **THEN** the destination words are that color in 15-bit before the chain continues

### Requirement: The album sample shows the captured pixels
A textured quad or sprite that samples the album scratch SHALL sample the words the capture wrote. Those words SHALL be in the texture before that primitive is drawn. The capture SHALL NOT create a second texture.

#### Scenario: Sprite after the copy
- **WHEN** a capture has written a known 15-bit color at `(896, 256)` and a sprite samples that cell
- **THEN** the pixel drawn for that cell is that color

### Requirement: The album read runs only for that copy
The back-buffer read that fills the album scratch SHALL run only while performing a copy that meets the album rule. It SHALL use only storage created with the host and SHALL NOT allocate or free memory per call or per frame.

#### Scenario: A frame with no album copy
- **WHEN** a frame is drawn and presented and no copy into `(896, 256, 64, 256)` runs
- **THEN** the scratch keeps the words it had, and that frame adds no allocation

### Requirement: A copy that misses the album rule is unchanged
A copy whose source does not overlap the draw buffer, or whose destination does not overlap `(896, 256, 64, 256)`, SHALL copy the words already in VRAM. `StoreImage` of `(896, 256, 64, 256)` SHALL return the words already stored there and SHALL NOT replace them with the frame.

#### Scenario: Source outside the frame
- **WHEN** the draw buffer holds a known color and a copy moves an 8 by 8 block from outside the draw buffer onto `(896, 256)`
- **THEN** the destination holds the words that were at the source

#### Scenario: Text row at the same x
- **WHEN** a copy moves a 16 by 1 row whose x is 896 and whose y is outside 256 through 511
- **THEN** that row holds the words that were at the source

#### Scenario: Storing the scratch
- **WHEN** the scratch is filled with a known word and `StoreImage` reads `(896, 256, 64, 256)` with no capture into it
- **THEN** the stored words are that known word
