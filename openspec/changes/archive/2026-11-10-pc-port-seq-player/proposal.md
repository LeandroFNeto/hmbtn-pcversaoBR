# Proposal

## Why

`pc-port-audio` plays the game's sound effects, but every scene is without music. The title, the farm, the minigames, and the staff roll start their background music with `sound_seq_start` → `SsSeqSetVol`, `SsSeqStop`, `SsSeqPlay(handle, 1, 0)`, after `sound_update` opened each loaded sequence with `SsSeqOpen(seq_data[i], seq_vabs[i])`. Those calls are no-ops on the host.

## What Changes

- `SsSeqOpen` validates a PS1 SEQ (`pQES` header: resolution in ticks per quarter note, tempo in microseconds per quarter note), copies its event stream into a host slot, and returns a handle bound to the VAB the game names. `SsSeqClose` frees it.
- A sequencer reads the SEQ's MIDI stream: variable-length delta times, running status, Note On / Note Off, Program Change, Control Change for volume (7), pan (10), and the libsnd loop markers (99 with 20 / 30, count in 6), the Tempo meta event (`FF 51`), and End of Track (`FF 2F`). Pitch bend and other events are read past and ignored.
- Time advances in audio samples: the SDL2 audio callback splits each buffer at the next event's sample, runs the due events, then mixes, so timing does not depend on `VSync` or the frame rate.
- A Note On keys on, through the `pc-port-audio` voice path, every tone of the channel's program whose key range holds the note, on free voices below the `SsSetReservedVoice` count, with velocity, channel volume, pan, and sequence volume. Note Off releases those voices.
- `SsSeqPlay` (play, loop count 0 = forever), `SsSeqStop`, `SsSeqPause`, `SsSeqReplay`, and `SsSeqSetVol` control a sequence from the game thread under the audio lock.
- `pc-port-audio`'s title scenario expected silence before Start. With music it no longer holds; that change's scenario is reworded to check the confirmation key-on after the press.
- No change under `src/`.

## Capabilities

### New Capabilities

- `host-seq-player`: The host opens the game's SEQ files and plays them on the host SPU with sample-accurate timing, loops, and the game's sequence controls.

### Modified Capabilities

- None. (`pc-port-audio` is not archived; its title scenario is edited in place.)

## Impact

- `src/pc_port/src/libsnd.c` (SEQ calls, voice lookup for notes), a new `src/pc_port/src/seq_player.c` with `src/pc_port/include/seq_player.h`, `spu_core.c` (the callback runs the sequencer before each mix slice), a new check file.
- No new package.
- Out of scope: pitch bend, vibrato, portamento, SysEx, reverb, SEP multi-sequence files, CD-XA streaming, and cross-channel modulation.
