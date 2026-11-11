# host-gl-present Specification

## Purpose
Presents an OpenGL framebuffer in the existing SDL window on every loop iteration, without taking the game sources into the host link.

## Requirements

### Requirement: The window still closes
The host SHALL keep a 640 by 480 window and SHALL end the process with code 0 when the window close event arrives, including while the game is waiting inside `VSync`.

#### Scenario: Close button
- **WHEN** the host executable is running and the window close control is activated
- **THEN** the loop ends and the process returns

#### Scenario: Close while the game waits
- **WHEN** the window close control is activated while `ps1_main` is inside a blocking `VSync`
- **THEN** the process exits with code 0

### Requirement: Each iteration presents OpenGL
Each vblank waited by a blocking `VSync` SHALL present the OpenGL framebuffer when something was drawn since the previous presentation, and SHALL clear the framebuffer after presenting.

#### Scenario: Frame is presented
- **WHEN** the host executable has been running for at least one blocking `VSync`
- **THEN** the window shows the OpenGL framebuffer from that vblank

### Requirement: No new third-party package
The host configuration SHALL obtain OpenGL from the system import library already available to MinGW. It SHALL NOT require a package other than the SDL2 directory the host build already uses.

#### Scenario: Configure with the existing SDL2 directory
- **WHEN** the host configuration is generated with the SDL2 directory that already links `hmbtn-host` and with no other library path
- **THEN** configuration completes and the host executable links

### Requirement: Game sources stay out of this link
Producing the presented window SHALL NOT require compiling the game or overlay translation units.

#### Scenario: Host target contents
- **WHEN** the host executable for this capability is linked
- **THEN** the link inputs are host sources under the PC port tree
- **AND** no game or overlay C file is a link input
