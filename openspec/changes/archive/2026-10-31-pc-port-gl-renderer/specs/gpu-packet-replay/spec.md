# Spec Delta

## Purpose

Replays Psy-Q ordering-table packets as OpenGL triangles so the same `DrawOTag` entry the game already calls can draw into the host window.

## ADDED Requirements

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
