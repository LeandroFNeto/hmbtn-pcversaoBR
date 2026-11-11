# host-forced-props Specification

## Purpose
Makes the shipping bin, the farm door, and the bed visible from colors the game already stored, when the texture sample is empty or the bed primitive was never submitted.

## Requirements

### Requirement: A measured hole uses the corner color
A textured primitive on the shipping-bin key or the farm-door key, with no replacement image for that key, whose sampled color word is `0x0000`, SHALL be drawn in the corner color stored in the packet. The fragment SHALL NOT be discarded.

#### Scenario: Shipping bin
- **WHEN** a primitive uses page `320,0`, its UV rectangle contains `(246, 242)`, no replacement image matches that page and UV, and the sampled color word is `0x0000`
- **THEN** a pixel of that primitive is the packet's corner color

#### Scenario: Farm door
- **WHEN** a primitive uses page `384,0`, its UV rectangle contains `(222, 159)`, no replacement image matches that page and UV, and the sampled color word is `0x0000`
- **THEN** a pixel of that primitive is the packet's corner color

#### Scenario: Any other empty sample stays discarded
- **WHEN** a primitive uses another page or another UV and the sampled color word is `0x0000`
- **THEN** that fragment is not drawn

#### Scenario: A replacement image still wins
- **WHEN** a replacement image matches the shipping-bin key or the farm-door key
- **THEN** a pixel of that primitive is the image, and the corner color is not used

### Requirement: The bed is drawn when its header size is known
Inside the house, the object nearest screen point `(254, 170)` that submits no primitive SHALL be drawn as an untextured polygon in the color stored in its model when that primitive's header code already has a byte size in the model's key table. The face test sign SHALL stay as it is.

#### Scenario: Known header
- **WHEN** the view is inside the house, the object nearest `(254, 170)` submits no primitive, and its header code has a byte size in the model's key table
- **THEN** that object is visible in the stored color and is not a solid black rectangle

#### Scenario: Unknown header
- **WHEN** that object's header code has no byte size in the model's key table
- **THEN** that object stays undrawn and every other object's draw is unchanged
