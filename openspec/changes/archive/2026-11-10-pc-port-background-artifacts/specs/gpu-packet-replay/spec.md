# Spec Delta

## ADDED Requirements

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
