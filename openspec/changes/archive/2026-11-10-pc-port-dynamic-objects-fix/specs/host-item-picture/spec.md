# Spec Delta

## Purpose

Keeps the pixels of an item picture that the in-memory lookup missed, from the disc sector the game already reads, through the upload that writes one VRAM row.

## ADDED Requirements

### Requirement: A non-zero source pixel survives the upload
The picture upload SHALL write a non-zero VRAM word when the source texel for that pixel is non-zero.

#### Scenario: One non-zero texel
- **WHEN** the upload is given a one-row picture whose texel is non-zero
- **THEN** the VRAM word at that pixel is not `0x0000`

### Requirement: A zero source stays zero
The picture upload SHALL write `0x0000` when the source texel is zero. It SHALL NOT substitute another color.

#### Scenario: One zero texel
- **WHEN** the upload is given a one-row picture whose texel is zero
- **THEN** the VRAM word at that pixel is `0x0000`

### Requirement: The shipping-bin sector is reported
`HMBTN_SELFTEST=1` SHALL read the disc sector at byte offset `0x3C19000 + (0x064 << 11)`, length `0x800`, and SHALL log that sector's first word and whether its image bytes are all zero.

#### Scenario: The sector line
- **WHEN** the self-check runs with the extracted disc
- **THEN** the host log has one line for id `0x064` naming the first word and whether the image is zero

### Requirement: A real picture replaces the hole
When the reported sector is an item picture whose image is not all zero, the shipping bin on the farm SHALL sample a non-zero word and SHALL NOT be a transparent hole.

#### Scenario: Farm bin after a non-zero sector
- **WHEN** the sector line names a picture with a non-zero image and the bin is on screen
- **THEN** the bin's sampled word is non-zero and the bin is not a transparent hole

### Requirement: A zero sector is left alone
When the reported sector is not an item picture, or its image bytes are all zero, the host SHALL NOT write a replacement color. The self-check SHALL still pass.

#### Scenario: Empty sector
- **WHEN** the sector line names a first word that is not an item picture, or an image that is all zero
- **THEN** no replacement color is written and the self-check passes

### Requirement: An absent id still misses
An item id that is not in the current resource list SHALL still miss the in-memory lookup. The disc sector above SHALL still be the one read for id `0x064`.

#### Scenario: Basket stays absent
- **WHEN** item `0x064` is looked up in the spring resource list
- **THEN** the lookup returns null and the following disc read uses `0x3C19000 + (0x064 << 11)`
