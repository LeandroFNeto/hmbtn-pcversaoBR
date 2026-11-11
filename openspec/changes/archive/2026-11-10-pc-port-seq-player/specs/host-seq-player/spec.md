# Spec Delta

## Purpose

Plays the game's PS1 SEQ music on the host SPU: opens the sequences the game loads, reads their MIDI events with sample-accurate timing, and answers the game's play, stop, pause, and volume calls.

## ADDED Requirements

### Requirement: SsSeqOpen accepts a PS1 SEQ
`SsSeqOpen(addr, vab)` SHALL check the `pQES` magic, read the resolution and the initial tempo from the header, copy the event stream up to its End of Track into a host slot, and return a handle from 0 to 15. A stream without the magic, without an End of Track within the slot size, or opened when every slot is in use, SHALL return −1. `SsSeqClose(handle)` SHALL stop the sequence and free the slot.

#### Scenario: Valid sequence
- **WHEN** the game opens a SEQ with the `pQES` magic and an End of Track
- **THEN** the call returns a handle from 0 to 15

#### Scenario: Not a SEQ
- **WHEN** the data does not start with `pQES`
- **THEN** the call returns −1

### Requirement: The event stream is read as PS1 MIDI
The sequencer SHALL read a variable-length delta time before each event, keep running status, and act on Note On, Note Off (and Note On with velocity 0), Program Change, Control Change 7 (volume) and 10 (pan), the Tempo meta event, and End of Track. Other channel, meta, and system events SHALL be skipped by their length.

#### Scenario: Running status
- **WHEN** a Note On is followed by two more notes without a status byte
- **THEN** three notes start

#### Scenario: Skipped event
- **WHEN** a pitch bend sits between two notes
- **THEN** both notes start and the pitch bend has no effect

### Requirement: Timing follows the sample clock
An event at tick `t` SHALL take effect at output sample `t × tempo × 44100 / (resolution × 1,000,000)` from the start, accumulated across tempo changes, independent of `VSync` and of the frame rate.

#### Scenario: One quarter note
- **WHEN** a sequence at resolution 480 and tempo 500,000 µs has a Note On at tick 480
- **THEN** the note starts 22,050 samples after play, within one sample

#### Scenario: Tempo change
- **WHEN** a Tempo event halves the tempo value
- **THEN** later ticks take half as many samples

### Requirement: Notes play the channel's program
A Note On SHALL key on every tone of the channel's current program in the sequence's VAB whose key range (minimum to maximum) holds the note, each on a free voice below the `SsSetReservedVoice` count, at that note's pitch, with volume from velocity, channel volume, sequence volume, and pan. A Note Off for the same channel and note SHALL release those voices.

#### Scenario: Note on and off
- **WHEN** a channel plays a Note On and later its Note Off
- **THEN** a voice reports key status on between them and enters release after the Note Off

#### Scenario: No free voice
- **WHEN** every voice below the reserved count is sounding
- **THEN** the note takes the voice that started earliest in that range

### Requirement: Sequences loop
A Control Change 99 with value 20 SHALL mark the loop start, with the loop count taken from the next Control Change 6 (0 meaning forever). A Control Change 99 with value 30 SHALL jump back to the loop start while loops remain. At End of Track, a sequence played with loop count 0 SHALL restart from its first event.

#### Scenario: Endless music
- **WHEN** a sequence played with `SsSeqPlay(h, 1, 0)` reaches its End of Track
- **THEN** it continues from its first event

#### Scenario: Marked loop
- **WHEN** a sequence has loop markers with count 0
- **THEN** it keeps returning to the loop start

### Requirement: The game controls a sequence
`SsSeqPlay(h, 1, count)` SHALL start the sequence from its beginning. `SsSeqStop(h)` SHALL stop it, release its voices, and rewind it. `SsSeqPause(h)` SHALL stop advancing it and release its voices; `SsSeqReplay(h)` SHALL continue from where it paused. `SsSeqSetVol(h, l, r)` SHALL scale its notes by `l / 127` and `r / 127`. These calls SHALL take the audio lock.

#### Scenario: Stop releases notes
- **WHEN** the game stops a sequence that is sounding
- **THEN** every voice it started enters release

#### Scenario: Pause and replay
- **WHEN** the game pauses a sequence and replays it
- **THEN** it continues from the event after the one where it paused

### Requirement: The title has music
With the game running from the extracted disc and an audio device, the title SHALL play its background music.

#### Scenario: Title music
- **WHEN** the title waits for Start with `HMBTN_AUDIO_DUMP` set
- **THEN** the log shows the title sequence opened and played, and the dump has sound during the seconds before any key is pressed
