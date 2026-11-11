# Spec Delta

## ADDED Requirements

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
