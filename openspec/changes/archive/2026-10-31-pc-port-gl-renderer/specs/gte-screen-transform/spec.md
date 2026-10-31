# Spec Delta

## Purpose

Projects vertices with the host GTE into the screen-space coordinates that GPU packets already store, using the same matrix, translation, offset, and screen distance the game sets.

## ADDED Requirements

### Requirement: Identity projection hits the offset
With an identity rotation, zero translation, screen offset (ox, oy), and a positive screen distance, projecting a vertex on the optical axis at a positive depth SHALL store screen coordinates (ox, oy).

#### Scenario: Vertex on the axis
- **WHEN** the rotation is identity, translation is zero, the offset is (160, 120), the screen distance is positive, and the vertex is (0, 0, z) with z greater than zero
- **THEN** the stored screen coordinate is (160, 120)

### Requirement: A vertex to the right moves right
With the same camera state, a vertex with a positive X and the same positive depth SHALL store a screen X greater than the offset and a screen Y equal to the offset.

#### Scenario: Vertex to the right of the axis
- **WHEN** the state from the identity scenario is kept and the vertex is (x, 0, z) with x and z greater than zero
- **THEN** the stored screen X is greater than 160
- **AND** the stored screen Y is 120

### Requirement: A vertex behind the camera is flagged
Projecting a vertex whose transformed depth is not positive SHALL report that the vertex is not in front of the camera.

#### Scenario: Behind the camera
- **WHEN** the rotation is identity, translation is zero, and the vertex depth is less than or equal to zero
- **THEN** the host GTE reports that the vertex failed the front-of-camera test

### Requirement: The result is written as packet words
The projected X and Y SHALL be stored as signed 16-bit screen coordinates in the memory the caller passes, which is the form `gte_stsxy` writes into a primitive.

#### Scenario: Store into a caller buffer
- **WHEN** a projection succeeds and the caller passes a destination for the screen coordinate
- **THEN** that destination holds the signed 16-bit X and Y

### Requirement: Host objects contain no MIPS GTE opcodes
Host compilation of the GTE implementation SHALL NOT emit MIPS coprocessor encodings.

#### Scenario: Host compile of the GTE unit
- **WHEN** the host GTE translation unit is compiled with MinGW GCC
- **THEN** compilation succeeds
- **AND** the object does not contain MIPS `.word` GTE opcodes
