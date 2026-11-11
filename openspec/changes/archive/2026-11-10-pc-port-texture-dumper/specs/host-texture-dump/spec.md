# Spec Delta

## Purpose

Writes one frame's textured primitives to BMP files named with the page and UV bounds a mods file already uses, using the colors stored for those texels.

## ADDED Requirements

### Requirement: One rising key writes the frame
While the texture dump is armed, one rising F8 SHALL write a BMP for each textured primitive of that frame. Holding F8 SHALL NOT write the next frame. An unarmed frame SHALL write nothing.

#### Scenario: A rising F8
- **WHEN** the dump is armed and F8 rises on a frame with one textured primitive
- **THEN** one BMP is written for that primitive

#### Scenario: F8 stays down
- **WHEN** F8 stays down for the next frame
- **THEN** that frame writes no BMP

#### Scenario: The dump is off
- **WHEN** the dump is not armed and F8 rises
- **THEN** no BMP is written

### Requirement: The file name is the page and UV bounds
The BMP name SHALL be the primitive's page and its UV bounds, in the form `pageX_pageY_u0_v0_u1_v1.bmp`.

#### Scenario: A known primitive
- **WHEN** a primitive with page `320,0` and UV bounds `0,0` to `7,7` is dumped
- **THEN** the file name is `320_0_0_0_7_7.bmp`

### Requirement: A pixel is the stored color
Each pixel SHALL be the color stored for that texel, including the CLUT when the primitive is 4-bit or 8-bit. A stored color of `0x0000` SHALL be black. The dump SHALL NOT write a different color in its place.

#### Scenario: A known CLUT color
- **WHEN** the stored color of that texel is a known non-zero value
- **THEN** the BMP pixel is that color

#### Scenario: A stored zero
- **WHEN** the stored color of that texel is `0x0000`
- **THEN** the BMP pixel is black

### Requirement: One name is written once
Two textured primitives in the same frame that share a page and UV bounds SHALL produce one BMP.

#### Scenario: A repeated key
- **WHEN** two primitives in the armed frame share a page and UV bounds
- **THEN** one BMP is written for that name

### Requirement: The dump does not replace the frame
Writing the BMP SHALL NOT change the pixels of the frame that was dumped, and SHALL NOT change the words stored in VRAM.

#### Scenario: The frame stays on VRAM
- **WHEN** an armed frame is dumped
- **THEN** that frame’s sampled word is the word it had before the dump, and the VRAM word is unchanged
