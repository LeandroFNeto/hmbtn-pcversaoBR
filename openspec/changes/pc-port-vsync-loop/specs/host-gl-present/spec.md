# Spec Delta

## MODIFIED Requirements

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
