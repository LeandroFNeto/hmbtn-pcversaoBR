# Spec Delta

## MODIFIED Requirements

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

## ADDED Requirements

### Requirement: ClearOTagR builds chains DrawOTag reads
`ClearOTagR(ot, n)` SHALL make each entry `ot[i]` for `i` from 1 to `n - 1` a zero-length tag linking to the 24-bit form of `ot[i - 1]`, and SHALL make `ot[0]` a zero-length tag whose link is `0x00FFFFFF`.

#### Scenario: Empty reverse table
- **WHEN** `ClearOTagR` is called on a table in PS1 RAM and `DrawOTag` starts from its last entry
- **THEN** the walk visits every entry down to `ot[0]` and stops without drawing
