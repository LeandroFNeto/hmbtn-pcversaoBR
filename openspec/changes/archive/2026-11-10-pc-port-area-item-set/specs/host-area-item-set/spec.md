# Spec Delta

## Purpose

Arms the shipping-bin and farm-door stand-in with the area the player entered, and keeps a neighboring polygon on the same page from taking that color.

## ADDED Requirements

### Requirement: The stand-in follows the active area
The shipping bin SHALL use the packet corner color only while the current area is the farm, the coop, or the barn. The farm door SHALL use the packet corner color only while the current area is the farm. Outside those areas the same primitive SHALL be discarded when its sample word is `0x0000`.

#### Scenario: Shipping bin on the farm
- **WHEN** the current area is the farm, a primitive uses the shipping bin's page and UV rectangle, no replacement image matches, and the sampled color word is `0x0000`
- **THEN** a pixel of that primitive is the packet's corner color

#### Scenario: Shipping bin in the coop and the barn
- **WHEN** the current area is the coop or the barn and the primitive is the shipping bin's rectangle, with the same empty sample and no replacement image
- **THEN** a pixel of that primitive is the packet's corner color

#### Scenario: Shipping bin outside its areas
- **WHEN** the current area is not the farm, the coop, or the barn, and the primitive is the shipping bin's rectangle with sample word `0x0000`
- **THEN** that fragment is not drawn

#### Scenario: Farm door on the farm only
- **WHEN** the current area is the farm, a primitive uses the farm door's page and UV rectangle, no replacement image matches, and the sampled color word is `0x0000`
- **THEN** a pixel of that primitive is the packet's corner color

#### Scenario: Farm door outside the farm
- **WHEN** the current area is not the farm and the primitive is the farm door's rectangle with sample word `0x0000`
- **THEN** that fragment is not drawn

### Requirement: A neighbor that only contains the point is not the item
A polygon whose UV rectangle contains the shipping bin's measured point, or the farm door's measured point, but is not that item's recorded rectangle SHALL stay discarded when the sample word is `0x0000`.

#### Scenario: The next tree on the bin page
- **WHEN** the current area is the farm and a primitive on the shipping bin's page has a larger UV rectangle that contains `(246, 242)`
- **THEN** that fragment is not drawn

#### Scenario: A replacement image still wins
- **WHEN** a replacement image matches the shipping bin's rectangle or the farm door's rectangle
- **THEN** a pixel of that primitive is the image, and the corner color is not used
