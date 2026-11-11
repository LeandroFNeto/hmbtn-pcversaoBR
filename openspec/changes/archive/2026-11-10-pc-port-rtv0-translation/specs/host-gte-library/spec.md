## ADDED Requirements

### Requirement: RTV0 adds the translation vector
The host `gte_rtv0` SHALL return rotation matrix times V0 plus the translation vector, shifted right by 12, as the game's command `0x4A480012` does. `gte_rtir` SHALL NOT add the translation vector.

#### Scenario: Object position is moved by the camera
- **WHEN** the rotation matrix is the identity, TR is (10, 20, 30), and V0 is (100, 200, 300)
- **THEN** `gte_rtv0` leaves IR1-3 at (110, 220, 330)

#### Scenario: Animated scene models are drawn
- **WHEN** the farm or the house draws a door, the chest, the shipping bin, or the bed through `scene_spin_entries`
- **THEN** the model is projected in front of the camera at its own position and its polygons reach the face test
