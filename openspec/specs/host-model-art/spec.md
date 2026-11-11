# host-model-art Specification

## Purpose
Points one polygon of a character or an object at an item picture, and leaves every model that has no such rule on the sample it calculated.

## Requirements

### Requirement: A look rule uses the item picture
On the places a look rule names, a polygon of that model whose page and UV equal the rule SHALL be drawn with the item picture named by the rule when that picture has a non-zero pixel and no replacement image matches. The polygon's vertices SHALL stay the ones the model produced. An object and the character follow the same rule.

#### Scenario: An object on its places
- **WHEN** the current area is one the object's rule names, the primitive is that model's logged page and UV, the item picture has a non-zero pixel, and no replacement image matches
- **THEN** a pixel of that primitive is the picture's color, and the vertices are the ones the model produced

#### Scenario: The character uses the same rule
- **WHEN** the current area is one the character's rule names, the primitive is that model's logged page and UV, the item picture has a non-zero pixel, and no replacement image matches
- **THEN** a pixel of that primitive is the picture's color, and the vertices are the ones the model produced

#### Scenario: The picture has no pixels
- **WHEN** the item picture named by the rule has no non-zero pixel
- **THEN** the polygon keeps the sample the model calculated and no substitute color is written

#### Scenario: Outside the rule's places
- **WHEN** the current area is not one the rule names
- **THEN** the polygon keeps the sample the model calculated

### Requirement: A model without a rule keeps its calculated sample
A model that has no look rule SHALL keep the sample it calculated, including when another model on the same page and UV has a rule.

#### Scenario: A second model
- **WHEN** two models emit the same page and UV and only the first has a look rule whose picture has a non-zero pixel
- **THEN** the second model's pixel is the sample it calculated

### Requirement: Another polygon of a ruled model keeps its calculated sample
A polygon of a ruled model that is not the rule's page and UV SHALL keep the sample the model calculated.

#### Scenario: The next polygon
- **WHEN** a primitive belongs to a ruled model and its page and UV are not the rule's
- **THEN** that primitive's sample is the one the model calculated

### Requirement: A replacement image still wins
A replacement image that matches the polygon's page and UV SHALL replace the sample, and the item picture SHALL NOT be used.

#### Scenario: An image matches the polygon
- **WHEN** a replacement image matches the ruled polygon's page and UV
- **THEN** a pixel of that primitive is the image, and the item picture is not used
