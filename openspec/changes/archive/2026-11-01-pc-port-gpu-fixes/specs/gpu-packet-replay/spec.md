# Spec Delta

## ADDED Requirements

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
