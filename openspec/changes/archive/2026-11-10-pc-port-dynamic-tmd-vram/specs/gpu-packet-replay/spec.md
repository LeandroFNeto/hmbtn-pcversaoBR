# Spec Delta

## ADDED Requirements

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
