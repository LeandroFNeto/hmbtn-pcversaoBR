# host-bin-picture Specification

## Purpose
Shows the shipping bin from the item picture the game already loads, and leaves every other polygon on the sample its model calculated.

## Requirements

### Requirement: The bin uses the item picture
On the farm, the coop, and the barn, the shipping bin polygon SHALL be drawn with the item picture loaded for id `0x064` when that picture contains a non-zero pixel and no replacement image matches the polygon. The polygon's vertices SHALL stay the ones the model produced.

#### Scenario: Farm, coop, and barn
- **WHEN** the current area is the farm, the coop, or the barn, the primitive is the shipping bin's logged page and UV, the item picture has a non-zero pixel, and no replacement image matches
- **THEN** a pixel of that primitive is the picture's color

#### Scenario: The picture file has no pixels
- **WHEN** the item picture for id `0x064` has no non-zero pixel
- **THEN** the shipping bin fragment is not drawn and no substitute color is written

#### Scenario: Outside those areas
- **WHEN** the current area is not the farm, the coop, or the barn
- **THEN** the shipping bin polygon keeps the sample the model calculated

### Requirement: A neighbor keeps its calculated sample
A polygon that is not the shipping bin's logged page and UV SHALL keep the sample the model calculated, including the tree on the same page.

#### Scenario: The next tree
- **WHEN** the current area is the farm and a primitive on the bin's page is not the bin's logged UV
- **THEN** that primitive's sample is the one the model calculated

#### Scenario: A replacement image still wins
- **WHEN** a replacement image matches the shipping bin polygon's page and UV
- **THEN** a pixel of that primitive is the image, and the item picture is not used
