# host-input-mapping Specification

## Purpose
Turns the PC's keyboard and game controller into the PlayStation pad's buttons and stick, sampled once per vblank, so the game reads PC input through its own pad code.

## Requirements

### Requirement: Input is sampled once per vblank after events
Each vblank a blocking `VSync` waits SHALL sample the keyboard and the game controller after window events are polled and before the game's next poll can read the buffer.

#### Scenario: Key held across a frame
- **WHEN** a mapped key is held while the game runs one frame of `VSync(2)`
- **THEN** the game's poll in that frame sees the button held

### Requirement: Keyboard layout
The host SHALL map arrows to the d-pad, `X` to ✕, `Z` to ○, `C` to □, `V` to △, `Enter` to Start, `Right Shift` to Select, `A` to L1, `S` to R1, `Q` to L2, and `W` to R2.

#### Scenario: Enter is Start
- **WHEN** `Enter` is held
- **THEN** the sampled pad has Start pressed and no other button

#### Scenario: Two keys
- **WHEN** `Up` and `X` are held together
- **THEN** the sampled pad has Up and ✕ pressed

### Requirement: Game controller layout
When a game controller is connected, the host SHALL map its south button to ✕, east to ○, west to □, north to △, left and right shoulders to L1 and R1, left and right triggers past half travel to L2 and R2, Back to Select, Start to Start, the d-pad to the d-pad, and the left and right sticks to the analog stick bytes, reporting the pad as analog. With no controller, the pad SHALL be digital.

#### Scenario: Controller south button
- **WHEN** a controller is connected and its south button is held
- **THEN** the sampled pad has ✕ pressed and the pad type is analog

#### Scenario: No controller
- **WHEN** no controller is connected
- **THEN** the pad type is digital and the stick bytes are `0x80`

### Requirement: Keyboard and controller combine
A button SHALL be pressed when it is pressed on the keyboard or on the controller.

#### Scenario: Both sources
- **WHEN** `Enter` is held on the keyboard and the controller's south button is held
- **THEN** the sampled pad has Start and ✕ pressed

### Requirement: Controllers connect while running
The host SHALL open the first game controller present at startup, open one plugged in later when none is open, and close it when it is unplugged, without stopping the game.

#### Scenario: Unplug
- **WHEN** the open controller is unplugged during play
- **THEN** the pad reports digital with only keyboard input and the game keeps running

### Requirement: Start leaves the title wait
With the game running from the extracted disc, pressing the Start input while the title waits SHALL advance the title, and pressing it again SHALL show the title menu.

#### Scenario: Title to menu
- **WHEN** the title is waiting and the `Enter` key is pressed and released twice, a second apart
- **THEN** the window shows the title menu instead of the waiting title
