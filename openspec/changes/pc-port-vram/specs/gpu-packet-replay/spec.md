# Spec Delta

## MODIFIED Requirements

### Requirement: Named quads and sprites become two triangles
`DrawOTag` SHALL draw every polygon command (`0x20`–`0x3F`) and every rectangle command (`0x60`–`0x7F`): three-vertex polygons as one triangle, four-vertex polygons and rectangles as two, using the screen coordinates stored in the packet. A flat packet SHALL use its single color. A gouraud packet SHALL use the color stored at each vertex. Rectangles of fixed size SHALL use 1 × 1, 8 × 8, or 16 × 16 as their command selects.

#### Scenario: Flat quad
- **WHEN** `DrawOTag` walks a `POLY_F4` whose four corners are inside the draw area
- **THEN** both triangles of that quad are drawn in the packet color

#### Scenario: Sprite
- **WHEN** `DrawOTag` walks an `SPRT` with a position and a width and height
- **THEN** that rectangle is drawn as two triangles

#### Scenario: Gouraud triangle
- **WHEN** `DrawOTag` walks a `POLY_G3`
- **THEN** one triangle is drawn with the three vertex colors

#### Scenario: Fixed-size tile
- **WHEN** `DrawOTag` walks a `TILE_16`
- **THEN** a 16 × 16 rectangle is drawn at its position

## REMOVED Requirements

### Requirement: Textures are not sampled yet
**Reason**: Textured packets now sample the host VRAM and their CLUT.
**Migration**: None. The requirement "Textured packets sample VRAM" replaces it.

## ADDED Requirements

### Requirement: Textured packets sample VRAM
A textured polygon or sprite SHALL read its texels from VRAM through its texture page (base x in units of 64, base y in units of 256, depth 4-bit, 8-bit, or 15-bit) and, for 4-bit and 8-bit pages, its CLUT (x in units of 16, y), at the packet's UV coordinates. A texel whose 16-bit color is `0x0000` SHALL NOT be drawn. A texel SHALL be multiplied by the packet color divided by 128, unless the packet's raw-texture bit is set.

#### Scenario: 4-bit sprite with a CLUT
- **WHEN** VRAM holds a 4-bit texture whose first texel is index 1 and a CLUT whose entry 1 is pure red, and a raw `SPRT` samples it
- **THEN** the first sprite pixel is red

#### Scenario: Transparent texel
- **WHEN** a textured packet samples a texel whose color is `0x0000`
- **THEN** the frame keeps the pixel that was there

#### Scenario: Modulation
- **WHEN** a textured packet with color (64, 64, 64) and the raw bit clear samples a white texel
- **THEN** the drawn pixel is about half intensity

### Requirement: Draw-mode words set the texture page
While walking a chain, `DrawOTag` SHALL keep the texture page and semi-transparency mode from the last `0xE1` word (in `DR_MODE`, `DR_TPAGE`, or any packet carrying it). Sprites and untextured primitives SHALL use that state. A textured polygon SHALL use and update it from its own texture-page field.

#### Scenario: Sprite after a DR_TPAGE
- **WHEN** a chain holds a `DR_TPAGE` selecting page (640, 0) at 4 bits, followed by an `SPRT`
- **THEN** the sprite samples page (640, 0) at 4 bits

### Requirement: Semi-transparency follows the PS1 modes
A primitive with its semi-transparency bit set SHALL blend with the frame by the current mode: 0 as half background plus half foreground, 1 as background plus foreground, 2 as background minus foreground, 3 as background plus a quarter of foreground. For a textured primitive, only texels with bit 15 set SHALL blend; other texels SHALL be drawn opaque.

#### Scenario: Fade quad
- **WHEN** a semi-transparent black `POLY_F4` in mode 2 covers a frame
- **THEN** the frame under it is unchanged

#### Scenario: Half blend
- **WHEN** a semi-transparent white `TILE` in mode 0 covers a black frame
- **THEN** the drawn pixels are about half intensity
