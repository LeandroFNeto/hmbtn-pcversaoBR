# Spec Delta

## Purpose

Builds the decompiled game as one native executable on Windows and Linux with a modern C compiler, leaving the PlayStation matching build in place.

## ADDED Requirements

### Requirement: Host configuration is explicit
The build system SHALL select the host executable only when the user asks for the host configuration. The default configuration SHALL remain the PlayStation matching build.

#### Scenario: Default configuration
- **WHEN** the project is configured with no host option
- **THEN** the PlayStation toolchain and the SHA-1 check of the matching binaries remain the build that runs

#### Scenario: Host configuration
- **WHEN** the project is configured with the host option
- **THEN** the host executable is the target that is built
- **AND** the PlayStation toolchain file is not required for that configuration

### Requirement: Modern host compilers
The host configuration SHALL compile the decompiled C sources with a modern GCC or Clang on Windows and on Linux.

#### Scenario: Windows host compile
- **WHEN** the host configuration is built on Windows with GCC or Clang
- **THEN** the decompiled C sources compile

#### Scenario: Linux host compile
- **WHEN** the host configuration is built on Linux with GCC or Clang
- **THEN** the decompiled C sources compile

### Requirement: One linked executable
The host configuration SHALL link the decompiled sources into one executable.

#### Scenario: Link succeeds
- **WHEN** the host configuration finishes building
- **THEN** one host executable exists
- **AND** the link step does not require the PSY-Q runtime libraries

### Requirement: Gameplay sources stay shared
The host configuration SHALL compile the same gameplay sources as the PlayStation build. Porting code SHALL live outside those sources.

#### Scenario: Unchanged game trees
- **WHEN** the host executable is produced
- **THEN** producing it does not require edits under the game and overlay source trees

### Requirement: Host build needs no PlayStation inputs
The host configuration SHALL build without a game disc image or a PSY-Q SDK install. The only third-party package it SHALL require is the SDL2 directory the host window already uses; OpenGL comes from the system library.

#### Scenario: Clean host configure
- **WHEN** the host configuration is generated on a machine that has a modern C compiler, CMake, and the SDL2 development package, and does not have the PSY-Q SDK or a disc image
- **THEN** configuration and the host executable build succeed

### Requirement: The host executable is 32-bit
The host configuration SHALL produce a 32-bit executable, because the decompiled sources store pointers in 32-bit integers and packet words.

#### Scenario: Pointer width
- **WHEN** a host translation unit evaluates `sizeof(void *)`
- **THEN** the result is 4

### Requirement: Launch is outside this capability
A successful host build SHALL NOT be treated as a requirement that the process runs, shows a window, or returns.

#### Scenario: Build without a run
- **WHEN** the host executable has been linked
- **THEN** the capability is satisfied whether or not the process is started
