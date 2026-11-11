# host-object-trace Specification

## Purpose
Records one chosen frame so a missing dynamic object can be told apart from a black packet, a geometry discard, a skipped GPU command, or an object that never entered its draw pass.

## Requirements

### Requirement: One keypress dumps one frame
With `HMBTN_OT_DUMP=1`, pressing F8 SHALL write the object trace of the next presented frame and then disarm. The same keypress without that variable SHALL write no object-trace lines. The black-region trace SHALL keep its own variable and its own schedule.

#### Scenario: Key without the switch
- **WHEN** `HMBTN_OT_DUMP` is unset and F8 is pressed
- **THEN** the host log gains no object-trace lines

#### Scenario: A second frame stays quiet
- **WHEN** `HMBTN_OT_DUMP=1` and F8 is pressed once
- **THEN** the log contains the object trace of that one presented frame
- **AND** the following presented frame adds no object-trace lines

### Requirement: The walk is counted by command class
The object trace SHALL count, for that frame, polygons drawn, rectangles drawn, line commands seen and not drawn, and every other command, including a mask-bit command. A line command that ends the rest of its packet SHALL be counted separately from a line that is the whole packet.

#### Scenario: Mixed list
- **WHEN** the dumped frame walks one polygon, one rectangle, one line packet, and one mask-bit command
- **THEN** the log reports one drawn polygon, one drawn rectangle, one skipped line, and one mask-bit command

#### Scenario: Line ends the packet
- **WHEN** a packet begins with a line command and further words follow it
- **THEN** the log reports that the line ended the packet

### Requirement: The view center names its primitive
For each primitive whose screen area contains the view center, the object trace SHALL log the command, the red, green, and blue, and, when the primitive is textured, the sampled 15-bit word. When no primitive contains the center, the trace SHALL say the center is uncovered.

#### Scenario: Black untextured center
- **WHEN** an untextured primitive covering the view center has color (0, 0, 0)
- **THEN** the log names that command and the color (0, 0, 0)

#### Scenario: Clear texel
- **WHEN** a textured primitive covering the view center samples word 0
- **THEN** the log names that command and word 0

#### Scenario: Nothing covers the center
- **WHEN** no primitive contains the view center
- **THEN** the log says the center is uncovered

### Requirement: Geometry calls are grouped by object
For the same frame, the object trace SHALL group geometry calls by the matrix push that places one scene object. Every group whose projected point lies in the central half of the view SHALL report that screen point, how many primitives failed the front-of-camera test, how many backface tests were not positive, how many were positive, and whether any primitive transform ran after the object's own projection. When no group lies in that half, the trace SHALL report the single nearest group instead.

#### Scenario: Backfaces only
- **WHEN** a group's projection lies in the central half and every backface test for it is not positive
- **THEN** the log reports that screen point, a non-zero count of failed backface tests, and a zero count of passing ones

#### Scenario: Draw pass never starts
- **WHEN** a group's projection lies in the central half and no primitive transform runs before the matching matrix pop
- **THEN** the log reports that the object did not enter its primitive pass

#### Scenario: Nothing in the central half
- **WHEN** every group's projection lies outside the central half
- **THEN** the log reports the group nearest the view center

### Requirement: Stored lights are logged and not applied
The object trace SHALL record the last flat-light colors and the last ambient color the game stored. Those calls SHALL leave packet colors unchanged.

#### Scenario: Light values appear
- **WHEN** flat light 0 is set to (208, 208, 208) and a frame is dumped
- **THEN** the log contains (208, 208, 208)

#### Scenario: Packet color wins
- **WHEN** a packet's stored color differs from the stored flat light and a frame is dumped
- **THEN** the drawn pixel matches the packet color

### Requirement: Each reported object closes with one outcome
For each group the geometry trace reports, the object trace SHALL write one line naming exactly one outcome of that group: a black emitted primitive, a geometry discard, a skipped line or mask command, an object that never entered its primitive pass, or none of these. The ground's primitive at the view center SHALL NOT decide the outcome of another group.

#### Scenario: Black primitive
- **WHEN** a primitive of that group covers the view center and its color is (0, 0, 0) or its sample is word 0
- **THEN** that group's line names a black emitted primitive

#### Scenario: Geometry discard
- **WHEN** that group entered its primitive pass and has no passing backface test
- **THEN** that group's line names a geometry discard

#### Scenario: Skipped command
- **WHEN** that group has a passing backface test, none of its primitives cover the view center, and the frame counted a line or a mask-bit command
- **THEN** that group's line names a skipped line or mask command

#### Scenario: Never entered
- **WHEN** that group did not enter its primitive pass
- **THEN** that group's line names an object that never entered its primitive pass

#### Scenario: No listed outcome
- **WHEN** a primitive of that group covers the view center and it is neither color (0, 0, 0) nor sample word 0
- **THEN** that group's line names none of the listed outcomes

#### Scenario: Uncovered without a listed cause
- **WHEN** that group has a passing backface test, none of its primitives cover the view center, and the frame counted neither a line nor a mask-bit command
- **THEN** that group's line names none of the listed outcomes

### Requirement: The dump leaves the picture and the results alone
Arming or writing the object trace SHALL NOT change presented pixels or the values geometry calls return.

#### Scenario: Same calls, switch off and on
- **WHEN** the same calls run once with `HMBTN_OT_DUMP` unset and once while a dump is written
- **THEN** the presented pixels match
- **AND** each geometry call returns the same value
