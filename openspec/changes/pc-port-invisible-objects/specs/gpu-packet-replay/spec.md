# Spec Delta

## ADDED Requirements

### Requirement: The chest, the shipping bin, and the doors are visible
With the game running from the extracted disc, the chest, the shipping bin, and the doors of the house and of the farm buildings SHALL show their own picture when the player is looking at them. Each of those objects SHALL NOT be a solid black rectangle of its own shape.

#### Scenario: Shipping bin
- **WHEN** a new game is in free play and the view shows the shipping bin
- **THEN** the shipping bin is visible, and its screen area is not a solid black rectangle

#### Scenario: Chest
- **WHEN** the view shows the chest
- **THEN** the chest is visible, and its screen area is not a solid black rectangle

#### Scenario: House door
- **WHEN** the view shows a door of the house
- **THEN** that door is visible, and its screen area is not a solid black rectangle

#### Scenario: Farm door
- **WHEN** the view shows a door of a farm building
- **THEN** that door is visible, and its screen area is not a solid black rectangle

### Requirement: The flashback does not draw those objects black
In the childhood flashback, the chest, the shipping bin, and the doors SHALL NOT appear as solid black rectangles.

#### Scenario: Flashback objects
- **WHEN** the childhood flashback is on screen and one of those objects is in the view
- **THEN** that object is not a solid black rectangle
