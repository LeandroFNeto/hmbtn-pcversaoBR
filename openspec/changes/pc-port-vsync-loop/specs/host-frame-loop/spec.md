# Spec Delta

## Purpose

Runs the PlayStation game loop on the host main thread and turns each of its vblank waits into a paced, presented, closable frame.

## ADDED Requirements

### Requirement: The game runs on the main thread
After the window, OpenGL, and the PS1 address space are ready, the host SHALL call `ps1_main` once on the main thread. The host SHALL NOT create a thread for it.

#### Scenario: Boot reaches VSync
- **WHEN** the host executable starts
- **THEN** `ps1_main` runs on the thread that created the window
- **AND** its boot `VSync(3)` calls are the first frames presented

### Requirement: Blocking VSync waits whole vblanks
`VSync(0)` SHALL wait one vblank and `VSync(n)` with `n >= 2` SHALL wait `n` vblanks, at 59.94 vblanks per second measured from the previous vblank deadline.

#### Scenario: Ten boot waits
- **WHEN** the game calls `VSync(3)` ten times in a row
- **THEN** about 30 vblanks pass, taking about half a second

### Requirement: Non-blocking VSync does not wait
`VSync(-1)` SHALL return the number of vblanks since startup without waiting. `VSync(1)` SHALL return without waiting or presenting.

#### Scenario: Counter read
- **WHEN** the game calls `VSync(-1)` twice with no blocking `VSync` between them
- **THEN** both calls return the same value

### Requirement: Each vblank runs the callback, events, and presentation
For each vblank a blocking `VSync` waits, the host SHALL call the function last passed to `VSyncCallback` once, then poll window events, then present the OpenGL framebuffer if a `DrawOTag` ran since the previous presentation.

#### Scenario: Frame drawn from the callback
- **WHEN** the registered callback calls `DrawOTag` during a vblank
- **THEN** that vblank presents what `DrawOTag` drew

#### Scenario: No callback registered
- **WHEN** a blocking `VSync` runs before any `VSyncCallback`
- **THEN** the vblank still polls events and paces

### Requirement: ResetCallback keeps an installed callback
`ResetCallback` SHALL clear the vblank callback only on its first call. Later calls SHALL leave the installed callback in place.

#### Scenario: Read after the callback is installed
- **WHEN** the game installs a vblank callback and then calls `ResetCallback` before a CD read
- **THEN** the next blocking `VSync` still calls that callback

### Requirement: The callback is not re-entered
A `VSync` called while the vblank callback is running SHALL return the vblank counter without waiting, polling, or presenting.

#### Scenario: VSync inside the callback
- **WHEN** the vblank callback calls `VSync(0)`
- **THEN** that call returns at once and the outer vblank continues

### Requirement: Closing the window ends the game cleanly
A window close event seen during a vblank SHALL stop the game without running more game code, destroy the OpenGL context and window, and exit with code 0.

#### Scenario: Close during boot
- **WHEN** the window is closed while the game is inside its boot `VSync(3)` calls
- **THEN** the process exits with code 0

### Requirement: GPU and CD waits do not hold the loop
`DrawSync` SHALL report an idle GPU. `CdControl`, `CdControlB`, and `CdRead` SHALL report success, and `CdReadSync` and `CdSync` SHALL report a completed transfer, all without waiting. What a read writes and which names `CdSearchFile` finds belong to the host CD drive.

#### Scenario: Read completion loop
- **WHEN** the game loops until `CdReadSync(0, 0)` reports done
- **THEN** the first call reports done

#### Scenario: GPU idle loop
- **WHEN** the game loops while `DrawSync(1)` is not zero
- **THEN** the first call returns zero and the loop ends
