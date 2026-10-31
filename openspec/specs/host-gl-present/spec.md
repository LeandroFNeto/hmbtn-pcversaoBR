# host-gl-present Specification

## Purpose
Presents an OpenGL framebuffer in the existing SDL window on every loop iteration, without taking the game sources into the host link.

## Requirements

### Requirement: The window still closes
The host loop SHALL keep a 640 by 480 window and SHALL leave the loop when the window close event arrives.

#### Scenario: Close button
- **WHEN** the host executable is running and the window close control is activated
- **THEN** the loop ends and the process returns

### Requirement: Each iteration presents OpenGL
Each pass of the host loop SHALL clear an OpenGL framebuffer and present it in the SDL window.

#### Scenario: Frame is presented
- **WHEN** the host executable has been running for at least one loop iteration
- **THEN** the window shows the OpenGL framebuffer from that iteration

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
