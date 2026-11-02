# gpu-packet-replay Specification

## Purpose
Replays Psy-Q ordering-table packets as OpenGL triangles so the same `DrawOTag` entry the game already calls can draw into the host window.

## Requirements

### Requirement: DrawOTag walks the ordering table
`DrawOTag` SHALL follow the packet chain from the tag it is given. A tag whose length is zero SHALL be treated as a link only. The walk SHALL stop at a null link.

#### Scenario: Empty table
- **WHEN** `DrawOTag` is called with a tag whose next pointer is null and whose length is zero
- **THEN** it returns without drawing a triangle

### Requirement: Named quads and sprites become two triangles
`DrawOTag` SHALL draw `POLY_F4`, `POLY_FT4`, `POLY_GT4`, and `SPRT` packets as two triangles each, using the screen coordinates stored in the packet. A flat packet SHALL use its single color. A gouraud packet SHALL use the color stored at each corner.

#### Scenario: Flat quad
- **WHEN** `DrawOTag` walks a `POLY_F4` whose four corners are inside the draw area
- **THEN** both triangles of that quad are drawn in the packet color

#### Scenario: Sprite
- **WHEN** `DrawOTag` walks an `SPRT` with a position and a width and height
- **THEN** that rectangle is drawn as two triangles

### Requirement: Unknown commands do not stop the walk
A packet whose command code is not one of the four named types SHALL be skipped. The walk SHALL continue at the next link.

#### Scenario: Skipped command
- **WHEN** `DrawOTag` walks a chain that contains a command other than `POLY_F4`, `POLY_FT4`, `POLY_GT4`, and `SPRT`, followed by a `POLY_F4`
- **THEN** the `POLY_F4` is still drawn

### Requirement: Textures are not sampled yet
A textured packet SHALL be drawn from its colors and screen corners. `DrawOTag` SHALL NOT sample VRAM or a file for that packet.

#### Scenario: Textured quad
- **WHEN** `DrawOTag` walks a `POLY_FT4` or `POLY_GT4`
- **THEN** the two triangles appear from the packet colors and corners
- **AND** no texture image is read

### Requirement: A host-built flat quad is visible
Until a game ordering table is submitted, each presented frame SHALL include one host-built `POLY_F4` drawn by `DrawOTag`, in a color distinct from the cleared framebuffer.

#### Scenario: Visible quad
- **WHEN** a frame has been presented
- **THEN** the window contains pixels of that quad's color

### Requirement: A texture window masks texture coordinates
While a texture window is in effect, a sampled texture coordinate SHALL keep the bits the window mask leaves free and SHALL take the window offset in the bits the mask covers. The mask and the offset are in steps of 8 pixels, as in GPU command `0xE2`. A mask of zero SHALL leave the coordinate unchanged. The window SHALL stay in effect until the next window command, including across later `DrawOTag` calls.

#### Scenario: Wrap inside eight pixels
- **WHEN** the window mask is 8 pixels in X, the offset is 0, and a sample's U is 9
- **THEN** the sample uses U 1

#### Scenario: Window off
- **WHEN** the window mask is zero
- **THEN** the sample uses the U and V from the packet

#### Scenario: Window survives the next list
- **WHEN** a list sets a non-zero window and the next `DrawOTag` has no window command
- **THEN** samples in that next list still use the same window

### Requirement: The farm and the flashback show their scenery
With the game running from the extracted disc, the area left of the farmhouse, the place of the bed inside the house, and the background of the childhood flashback SHALL show scenery, not a solid black rectangle.

#### Scenario: Beside the house
- **WHEN** a new game is in free play and the view shows the farmhouse
- **THEN** the region left of the house is not a solid black rectangle

#### Scenario: The bed
- **WHEN** the view inside the house shows the place of the bed
- **THEN** that place is not a solid black rectangle

#### Scenario: Flashback
- **WHEN** the childhood flashback is on screen
- **THEN** its background is not a solid black rectangle
