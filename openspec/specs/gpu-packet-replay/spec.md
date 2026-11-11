# gpu-packet-replay Specification

## Purpose
Replays Psy-Q ordering-table packets as OpenGL triangles so the same `DrawOTag` entry the game already calls can draw into the host window.

## Requirements

### Requirement: DrawOTag walks the ordering table
`DrawOTag` SHALL follow the packet chain from the tag it is given. A tag whose length is zero SHALL be treated as a link only. A 24-bit link below `0x00200000` SHALL name the PS1 RAM address `0x80000000` plus that offset. A link from `0x00400000` up to `0x01000000` SHALL name that host image address. The walk SHALL stop at a null link, at `0x00FFFFFF`, and at any other link value without reading through it.

#### Scenario: Empty table
- **WHEN** `DrawOTag` is called with a tag whose next pointer is null and whose length is zero
- **THEN** it returns without drawing a triangle

#### Scenario: Packet in PS1 RAM
- **WHEN** a `POLY_F4` is stored at `0x801C0000` and the tag `DrawOTag` starts from links to it with the 24-bit value `0x001C0000`
- **THEN** that quad is drawn

#### Scenario: Terminator
- **WHEN** the chain reaches a tag whose link is `0x00FFFFFF`
- **THEN** the walk stops after that tag

#### Scenario: Link outside both windows
- **WHEN** a tag links to `0x00300000`
- **THEN** the walk stops without reading that address

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

### Requirement: Unknown commands do not stop the walk
A packet whose command code is not one of the four named types SHALL be skipped. The walk SHALL continue at the next link.

#### Scenario: Skipped command
- **WHEN** `DrawOTag` walks a chain that contains a command other than `POLY_F4`, `POLY_FT4`, `POLY_GT4`, and `SPRT`, followed by a `POLY_F4`
- **THEN** the `POLY_F4` is still drawn

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

### Requirement: Blend rate 2 subtracts the source
A semi-transparent primitive at blend rate 2 SHALL read back as the destination minus the source, clamped at zero. When that readback is already the difference, the blend equation SHALL stay as it is. When the readback is the sum or the unchanged destination, the same pair SHALL be drawn again as the difference. This requirement SHALL NOT change rate 0, rate 1, or rate 3, and SHALL NOT write a replacement color.

#### Scenario: White over white
- **WHEN** a semi-transparent untextured quad of `rgb 255,255,255` at blend rate 2 covers a pixel of `rgb 255,255,255`
- **THEN** the readback is `rgb 0,0,0`

#### Scenario: White over a darker pixel
- **WHEN** that quad at blend rate 2 covers a pixel of `rgb 40,0,0`
- **THEN** the readback is `rgb 0,0,0`

#### Scenario: The difference is already in effect
- **WHEN** the white-over-white readback is already `rgb 0,0,0`
- **THEN** the blend equation is not changed

#### Scenario: The readback is the sum or the destination
- **WHEN** the white-over-white readback is `rgb 255,255,255`
- **THEN** the same pair is drawn again and the readback is `rgb 0,0,0`

### Requirement: A background frame names the edge primitive
One house frame, scene `0x50` through `0x5B`, and one flashback frame, `place 54` and scene `0xe6` or `0xe7`, SHALL each record the last primitive whose triangle contains `(40,40)`, `(280,40)`, `(40,200)`, `(280,200)`, and `(160,120)`. The record SHALL name the command, the raw color, the semi-transparency bit, and the blend rate. A point with no primitive SHALL be recorded as none. The pixel SHALL NOT be replaced.

#### Scenario: Two primitives cover one point
- **WHEN** two primitives contain one of those five points
- **THEN** the record names the later primitive

#### Scenario: No primitive covers the point
- **WHEN** no primitive contains the point
- **THEN** the record says none

#### Scenario: The frame is another scene
- **WHEN** the frame is not a house scene and not that flashback
- **THEN** that frame writes no edge record

#### Scenario: The named color stays
- **WHEN** the record names `rgb 255,255,255`
- **THEN** the pixel is not recolored

### Requirement: A large untextured quad on that frame is listed
On each frame that writes an edge record, an untextured quad whose width is at least 160 or whose height is at least 120 SHALL be listed with its command, bounds, raw color, semi-transparency bit, and blend rate.

#### Scenario: A full-screen untextured quad
- **WHEN** an untextured quad on that frame covers at least 160 by 120
- **THEN** the record lists that quad's command, bounds, raw color, semi-transparency bit, and blend rate

#### Scenario: A small quad
- **WHEN** an untextured quad on that frame is smaller than 160 in width and smaller than 120 in height
- **THEN** it is not listed as a large quad

### Requirement: Clears on that frame are recorded
On each frame that writes an edge record, every clear of a VRAM rectangle SHALL be recorded with its rectangle and color. When a clear of the window follows the primitive recorded for a point, that point's record SHALL say the clear came after. The clear SHALL NOT be given a different color.

#### Scenario: A clear runs
- **WHEN** a VRAM rectangle is cleared during the frame
- **THEN** the record names that rectangle and its color

#### Scenario: The window is cleared after the primitive
- **WHEN** a clear of the window runs after the primitive recorded for one of the five points
- **THEN** that point's record says the clear came after

### Requirement: A mask word on that frame is recorded
On each frame that writes an edge record, every mask command SHALL be recorded with its word. The color write mask and the stencil write mask SHALL be recorded as they already are. The host SHALL NOT change either mask. When the frame has no mask command and every color channel is writable, the record SHALL say so.

#### Scenario: A mask command is present
- **WHEN** the frame contains a mask command
- **THEN** the record names its word and the color write mask is unchanged

#### Scenario: No mask command
- **WHEN** the frame has no mask command and every color channel is writable
- **THEN** the record says so and no mask is added

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

### Requirement: ClearOTagR builds chains DrawOTag reads
`ClearOTagR(ot, n)` SHALL make each entry `ot[i]` for `i` from 1 to `n - 1` a zero-length tag linking to the 24-bit form of `ot[i - 1]`, and SHALL make `ot[0]` a zero-length tag whose link is `0x00FFFFFF`.

#### Scenario: Empty reverse table
- **WHEN** `ClearOTagR` is called on a table in PS1 RAM and `DrawOTag` starts from its last entry
- **THEN** the walk visits every entry down to `ot[0]` and stops without drawing

### Requirement: A stored texel is the word the packet samples
A textured primitive SHALL sample the 15-bit color the game stored at the VRAM address that primitive names. When that stored word is non-zero, the primitive SHALL NOT be a transparent hole. When that stored word is `0x0000`, the primitive SHALL stay transparent.

#### Scenario: The chest samples its loaded texel
- **WHEN** the chest fills the middle of the view and the game stored a non-zero color at the address the chest's textured primitive names
- **THEN** the sampled word is that non-zero color and the chest is not a transparent hole

#### Scenario: The shipping bin samples its loaded texel
- **WHEN** the shipping bin fills the middle of the view and the game stored a non-zero color at the address the bin's textured primitive names
- **THEN** the sampled word is that non-zero color and the bin is not a transparent hole

#### Scenario: A door samples its loaded texel
- **WHEN** a house door or a farm-building door fills the middle of the view and the game stored a non-zero color at the address that door's textured primitive names
- **THEN** the sampled word is that non-zero color and the door is not a transparent hole

#### Scenario: A stored transparent word stays transparent
- **WHEN** the word stored at the address a textured primitive names is `0x0000`
- **THEN** the primitive stays transparent and no replacement color is invented

### Requirement: The flashback uses the same texel
The childhood flashback SHALL use the same sampling result as the farm frame for the chest, the shipping bin, and the doors.

#### Scenario: Flashback objects keep the farm texel
- **WHEN** the childhood flashback shows the chest, the shipping bin, or a door, and the farm frame samples a non-zero word for that object
- **THEN** the flashback does not draw that object as a transparent hole

#### Scenario: A still-transparent flashback stays recorded
- **WHEN** the farm objects sample a non-zero word and the flashback still draws them as transparent holes
- **THEN** that flashback frame's sample record is kept and no separate fix is opened for the flashback alone

### Requirement: The chest, the shipping bin, and the doors are visible
With the game running from the extracted disc, the chest, the shipping bin, and the doors of the house and of the farm buildings SHALL show their own picture when the player is looking at them. Each of those objects SHALL NOT be a solid black rectangle of its own shape.

#### Scenario: Shipping bin
- **WHEN** a new game is in free play and the view shows the shipping bin
- **THEN** the shipping bin is visible, and its screen area is not a solid black rectangle

#### Scenario: Chest
- **WHEN** the view shows the chest
- **THEN** the chest is visible, and its screen area is not a solid black rectangle

#### Scenario: House door
- **WHEN** the view shows a door of the house
- **THEN** that door is visible, and its screen area is not a solid black rectangle

#### Scenario: Farm door
- **WHEN** the view shows a door of a farm building
- **THEN** that door is visible, and its screen area is not a solid black rectangle

### Requirement: The flashback does not draw those objects black
In the childhood flashback, the chest, the shipping bin, and the doors SHALL NOT appear as solid black rectangles.

#### Scenario: Flashback objects
- **WHEN** the childhood flashback is on screen and one of those objects is in the view
- **THEN** that object is not a solid black rectangle

### Requirement: An untextured semi-transparent quad blends
An untextured quad whose semi-transparency bit is set SHALL blend with the pixel already in the frame. The same quad with that bit clear SHALL stay the packet color. The host SHALL NOT write a color other than that blend or that packet color.

#### Scenario: Semi over a bright pixel
- **WHEN** a semi-transparent untextured quad of `rgb 20,0,0` covers a known bright pixel
- **THEN** the readback is the blend of the packet color and that bright pixel

#### Scenario: The semi bit is clear
- **WHEN** the same quad is drawn with the semi-transparency bit clear
- **THEN** the readback is `rgb 20,0,0`

#### Scenario: The blend is already in effect
- **WHEN** the semi-transparent quad already reads back as that blend
- **THEN** no second blend is applied

#### Scenario: The farm center is not this quad
- **WHEN** the traced center left of the house is a textured sprite
- **THEN** that sprite's sample is left as it was

### Requirement: A front face at the bed is drawn
A projected group at the bed center whose backface test passes SHALL be drawn. A group that never reaches that center SHALL be recorded, and the game's scene selection SHALL stay as it is.

#### Scenario: A passing backface test
- **WHEN** a group whose screen point is the bed center passes the backface test
- **THEN** that group is drawn

#### Scenario: The group never enters
- **WHEN** no projected group falls on the bed center
- **THEN** the frame is recorded and the game's scene selection is unchanged

#### Scenario: The backface sign is reversed
- **WHEN** a known front face fails the backface test and a known back face passes it
- **THEN** the front face passes and the back face fails

### Requirement: A flashback frame records its background
One childhood-flashback frame SHALL record the clear color the frame started from, the command and color of an untextured quad that covers the background center, and whether a VRAM copy covers the draw buffer. The record SHALL NOT replace that color with another.

#### Scenario: The recorded gray quad
- **WHEN** the background center is an untextured quad of `rgb 107,107,107`
- **THEN** the record names that command and color, and the pixel stays `rgb 107,107,107`

#### Scenario: The clear differs from the stored color
- **WHEN** the frame starts from a color other than the clear color the game stored
- **THEN** the frame starts from the stored color
