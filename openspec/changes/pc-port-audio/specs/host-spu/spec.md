# Spec Delta

## Purpose

Gives the host a PlayStation sound processor: sound RAM, ADPCM voices with the PS1 envelope, and a mixer that plays through the PC's audio device.

## ADDED Requirements

### Requirement: Sound RAM is 512 KB
The host SHALL keep 512 KB of sound RAM, zero at startup. A write SHALL copy bytes to a given sound-RAM address and SHALL NOT write past its end.

#### Scenario: Write and read back
- **WHEN** 32 bytes are written at sound-RAM address `0x1010`
- **THEN** reading 32 bytes from `0x1010` returns them

#### Scenario: Write past the end
- **WHEN** a write would cross 512 KB
- **THEN** only the bytes below 512 KB are written

### Requirement: ADPCM blocks decode as the PS1 does
A voice SHALL decode 16-byte ADPCM blocks: byte 0 holds the shift (low nibble) and filter (high nibble), byte 1 the flags, and 14 bytes hold 28 four-bit samples, each expanded with the shift and the filter coefficient pairs (0, 0), (60, 0), (115, −52), (98, −55), (122, −60) over the two previous samples, divided by 64, and clamped to 16 bits.

#### Scenario: Known block
- **WHEN** a block with shift 12, filter 0, and nibbles 1, 7, 8, 15 is decoded from zero history
- **THEN** the samples are 1, 7, −8, and −1

### Requirement: Loop flags control the voice
At the end of a block whose flag bit 0 is set, a voice SHALL continue at the last block that had flag bit 2 set when flag bit 1 is also set, and SHALL otherwise stop and report itself off.

#### Scenario: One-shot sample
- **WHEN** a voice plays a sample whose last block has flags `0x01`
- **THEN** after that block the voice is off and silent

#### Scenario: Looping sample
- **WHEN** a voice plays a sample whose first block has flag `0x04` and last block flags `0x03`
- **THEN** the voice keeps sounding from the first block until it is keyed off

### Requirement: The envelope follows the PS1 ADSR
Each voice SHALL apply the PS1 ADSR envelope given by its two 16-bit ADSR words: attack, decay to the sustain level, sustain, and release on key off, with linear and exponential modes and the rate encoding of the PS1 SPU. A voice whose envelope reaches zero in release SHALL report itself off.

#### Scenario: Release after key off
- **WHEN** a sounding voice is keyed off
- **THEN** its output falls to zero and it then reports off

### Requirement: Pitch and volume
A voice SHALL advance through its samples at its pitch divided by 4096 per output sample (4096 plays at 44,100 Hz) with linear interpolation, and SHALL scale its output by its left and right volumes and the master volume.

#### Scenario: Octave up
- **WHEN** a voice plays at pitch 8192
- **THEN** it consumes two source samples per output sample

### Requirement: The mixer plays on the PC
The host SHALL open an SDL2 audio device at 44,100 Hz, 16-bit signed, stereo, and SHALL fill it from a callback that sums the active voices and clamps to 16 bits. If no device opens, the host SHALL log it and keep running silent.

#### Scenario: No audio device
- **WHEN** the audio device cannot be opened
- **THEN** the game keeps running and voice status calls still work

### Requirement: Audio is thread safe and allocation free
The game thread SHALL change voice and sound-RAM state only while holding the audio device lock. The audio callback SHALL NOT allocate or free memory. The device SHALL be closed, and the callback stopped, before the host exits.

#### Scenario: Long run
- **WHEN** the game runs for one minute playing sounds
- **THEN** the process's private memory at the end is within 1 MB of its value after the first ten seconds
- **AND** closing the window exits with code 0
