# Spec Delta

## Purpose

Records why an in-memory item-art lookup returned null for the shipping bin or for an item loaded in the house, without changing the pointer the game uses or the disc read that follows.

## ADDED Requirements

### Requirement: A watched miss writes one line
A null in-memory item-art lookup SHALL write one host-log line when the item id is `0x064`, and when the current mode scene is a house scene (`0x50` through `0x5B`). Any other null lookup SHALL write nothing. A second null lookup of the same id SHALL NOT write another line.

#### Scenario: Shipping bin misses
- **WHEN** the lookup of item `0x064` returns null
- **THEN** the host log gains one line for id `0x064`

#### Scenario: House miss
- **WHEN** the lookup of another id returns null while the mode scene is from `0x50` through `0x5B`
- **THEN** the host log gains one line naming that id

#### Scenario: A hit stays quiet
- **WHEN** the lookup of item `0x064` returns an address
- **THEN** the host log gains no item-art line for that id

#### Scenario: A repeat stays quiet
- **WHEN** item `0x064` misses twice
- **THEN** the host log contains one line for that id

### Requirement: The line names the exit and the table words
The line SHALL name the item id, the exit that made the lookup return null, the table-of-contents pointer that entry indexes, that entry's count, and its first offset. The exits SHALL be a missing mode, a resource set of -1, a resource set outside 0..9, a null id list, and an id that is not in the list. A resource set outside 0..9 SHALL NOT be used as an index. When the count is outside 1..63, the line SHALL still name the count and SHALL say the first offset was not read.

#### Scenario: Id absent from the list
- **WHEN** the mode and the id list are present and item `0x064` is not in the list
- **THEN** the line names that exit, the list's count word, the table pointer, the count, and the first offset

#### Scenario: Resource set out of range
- **WHEN** the resource set is outside 0..9
- **THEN** the line names that exit and does not report a table pointer

#### Scenario: Count out of range
- **WHEN** the indexed table's count is outside 1..63
- **THEN** the line names that count and says the first offset was not read

### Requirement: The lookup and the disc read stay the same
Noting a miss SHALL leave the lookup's returned pointer unchanged. When that pointer is null, the game SHALL still read the same disc sector it reads today.

#### Scenario: Null still reads the disc
- **WHEN** the lookup of item `0x064` returns null
- **THEN** the game still reads sector `0x3C19000 + (0x064 << 11)`

#### Scenario: A hit is the same pointer
- **WHEN** the lookup finds the item
- **THEN** the pointer the game uses is the one the lookup returned
