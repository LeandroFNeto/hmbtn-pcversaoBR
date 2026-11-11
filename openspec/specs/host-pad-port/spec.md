# host-pad-port Specification

## Purpose
Emulates the PlayStation pad library the game uses, so the game sees a connected standard controller on port 1 and reads its state from the receive buffer it registered.

## Requirements

### Requirement: Port 1 is a connected pad
`PadGetState` SHALL return `PadStateStable` for port `0x00` once `PadStartCom` has run, and `PadStateDiscon` for port `0x10` and for any port before `PadStartCom`.

#### Scenario: After start
- **WHEN** the game has called `PadInitDirect` and `PadStartCom`
- **THEN** `PadGetState(0x00)` returns `PadStateStable`
- **AND** `PadGetState(0x10)` returns `PadStateDiscon`

### Requirement: Mode and actuator queries answer as a standard pad
`PadInfoMode(port, InfoModeCurExID, 0)` SHALL return 0 for a digital pad and `0x73` for an analog pad. `PadSetAct` SHALL accept the game's table and SHALL NOT write it. `PadSetActAlign` SHALL return 1 for port `0x00`.

#### Scenario: Pad setup
- **WHEN** `pad_poll_ports` sets up port 1 for the first time
- **THEN** `PadSetActAlign` returns 1 and the next poll reads the buttons

### Requirement: The receive buffer header
Each vblank, the host SHALL write the port 1 buffer passed to `PadInitDirect` with byte 0 status `0x00`, byte 1 `0x41` for a digital pad or `0x73` for an analog pad, and bytes 4 to 7 the right X, right Y, left X, and left Y stick positions, `0x80` when centered. The port 2 buffer SHALL have status `0xFF`.

#### Scenario: Digital pad
- **WHEN** no controller is connected at a vblank
- **THEN** byte 0 is `0x00`, byte 1 is `0x41`, and bytes 4 to 7 are `0x80`

#### Scenario: Port 2
- **WHEN** the host writes the buffers
- **THEN** byte 0 of the port 2 buffer is `0xFF`

### Requirement: The button bytes are active low
Bytes 2 and 3 of the port 1 buffer SHALL hold the buttons with 0 meaning pressed. Byte 2: Select `0x01`, Start `0x08`, Up `0x10`, Right `0x20`, Down `0x40`, Left `0x80`. Byte 3: L2 `0x01`, R2 `0x02`, L1 `0x04`, R1 `0x08`, △ `0x10`, ○ `0x20`, ✕ `0x40`, □ `0x80`.

#### Scenario: Nothing pressed
- **WHEN** no input is held at a vblank
- **THEN** bytes 2 and 3 of the port 1 buffer are `0xFF`

#### Scenario: Start held
- **WHEN** the input mapped to Start is held at a vblank
- **THEN** bit `0x08` of byte 2 is 0 and every other button bit is 1
- **AND** the game's `input_pad1->held` has `PAD_START` after its next poll

### Requirement: The pad never writes before PadInitDirect
The host SHALL NOT write any buffer until `PadInitDirect` has given it one, and SHALL write only the registered buffers' first 8 bytes.

#### Scenario: Early vblank
- **WHEN** a vblank happens before the game calls `PadInitDirect`
- **THEN** no game memory is written by the pad emulation
