# Spec Delta

## ADDED Requirements

### Requirement: Back color and light matrices are kept
`SetBackColor` SHALL store its three components, and `SetColorMatrix` and `SetLightMatrix` SHALL store the nine entries of the matrix they are given. A later read SHALL return those stored values. Emitting a polygon packet SHALL NOT replace that packet's corner colors with a color computed from these registers.

#### Scenario: Values round-trip
- **WHEN** `SetBackColor(10, 20, 30)` is called and each matrix setter is called with its own nine entries
- **THEN** the back color reads (10, 20, 30) and each matrix reads back as written

#### Scenario: Packet colors win
- **WHEN** a gouraud packet with known corner colors is drawn after those registers were set to different colors
- **THEN** the drawn corners use the packet's colors
