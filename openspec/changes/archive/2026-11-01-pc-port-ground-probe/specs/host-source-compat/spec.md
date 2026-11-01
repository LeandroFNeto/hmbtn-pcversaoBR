# Spec Delta

## ADDED Requirements

### Requirement: A ground probe that finds nothing reads what the PS1 reads
Where the game reads the ground hit primitive without testing it for null, and the probe found no ground, the host SHALL read the bytes the PS1 reads at that offset from address 0, so the game's own "no ground" handling runs. Reads that already test the hit for null SHALL stay unchanged.

#### Scenario: Player probe off the ground
- **WHEN** the tile ahead of the player has no ground and the player's status word is built from the hit
- **THEN** the word read is the PS1's word 3 at address 0, the status becomes 0x8000, and the game goes on

#### Scenario: Probe sweep misses a cell
- **WHEN** a field, greenhouse, or mine probe sweep finds no ground at a cell
- **THEN** the cell's quad stays empty, as on the PS1, and the sweep goes on

#### Scenario: Hit found
- **WHEN** the probe finds ground
- **THEN** every covered read goes through the hit exactly as the original code does
