# host-crash-report Specification

## Purpose
Lets whoever runs the host find where a crash happened from the host's log alone, without reproducing it under a debugger.

## Requirements

### Requirement: A fault logs its call stack
When the host process faults, the log SHALL hold, before the process ends, the fault code, the faulting instruction's address, the accessed address, and the return addresses of up to 16 calling frames, innermost first, each in a form `addr2line` accepts.

#### Scenario: Null read in game code
- **WHEN** game code reads through a null pointer the host does not cover
- **THEN** the log has a fault line with the accessed address below 0x10000, followed by a stack line whose addresses `addr2line -e hmbtn-host.exe` resolves to the faulting function's callers

#### Scenario: Broken frame chain
- **WHEN** a frame pointer on the stack is outside the stack or not word-aligned
- **THEN** the stack line ends at the last valid frame and the process still ends with the fault logged
