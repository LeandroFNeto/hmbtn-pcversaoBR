# Spec Delta

## Purpose

Points the shipping bin's one textured sample at the VRAM address where its picture stored a non-zero word, when exactly one such address is known.

## ADDED Requirements

### Requirement: The sample address is named
The host SHALL name the VRAM address sampled by command `0x2c` at page `320,0` and UV `246,242` in 4 bits as `(381, 242)`.

#### Scenario: The known primitive
- **WHEN** that page, UV, and depth are sampled
- **THEN** the address is x `381` and y `242`

### Requirement: A stored row is named
Each row stored for the `0x064` picture SHALL be logged with its rectangle and whether any word in that row is non-zero.

#### Scenario: A row away from the sample
- **WHEN** a non-zero word is stored in a row that does not cover `(381, 242)`
- **THEN** the log names that rectangle and says the word is non-zero

### Requirement: One outside row moves the page
When exactly one non-zero row does not cover `(381, 242)`, the sampler SHALL add that row's delta to command `0x2c` with page `320,0` and UV `246,242`, and SHALL leave every other primitive unchanged. The word then read SHALL be that non-zero word.

#### Scenario: One row supplies the delta
- **WHEN** the log names one non-zero row outside `(381, 242)` and no other non-zero row
- **THEN** the bin sample reads that row's non-zero word and a different command is unchanged

### Requirement: One inside word moves the UV
When the row that covers `(381, 242)` stores zero at that pixel and exactly one other word in that picture is non-zero, the sampler SHALL move the UV of that same primitive by the delta to that word. The word then read SHALL be that non-zero word.

#### Scenario: One word inside the picture
- **WHEN** the sample pixel is zero and the log names exactly one other non-zero word
- **THEN** the bin sample reads that word

### Requirement: Any other count stops
When the picture stores no non-zero word, or more than one candidate row or word, the host SHALL NOT move the sample and SHALL NOT write a replacement color.

#### Scenario: No single candidate
- **WHEN** the log names zero non-zero words, or more than one candidate
- **THEN** the sample stays at `(381, 242)` and no replacement color is written

### Requirement: The lookup stays null
The in-memory lookup of item `0x064` SHALL still return null. The disc sector SHALL still be `0x3C19000 + (0x064 << 11)`.

#### Scenario: Basket stays absent
- **WHEN** item `0x064` is looked up in the spring resource list
- **THEN** the lookup returns null and the disc read uses that sector
