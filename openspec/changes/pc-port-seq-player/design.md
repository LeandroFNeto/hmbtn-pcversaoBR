# Design

## Context

See proposal.md for why. What the game does, from `src/main/sound/` and the overlays:

- `sound_load_step` copies each SEQ whole from A_FILE member 9 into its sound buffer (`seq_data[slot]`) and records the VAB it plays (`seq_vabs[slot]`). `sound_update` opens every loaded slot with `SsSeqOpen(seq_data[i], seq_vabs[i])` and calls `SsStart` once.
- `sound_seq_start` (title, farm, minigames, staff roll, swind) does `SsUtAllKeyOff(0)`, `SsSeqSetVol(h, l, r)`, `SsSeqStop(h)`, `SsSeqPlay(h, 1, 0)`. Fades call `SsSeqSetVol` every frame; scene changes call `SsSeqStop`, `SsSeqPause`/`SsSeqReplay`, and `SsSeqClose`.
- `sound_init` sets `SsSetTickMode(1)` (60 Hz ticks on the PS1) and `SsSetReservedVoice(0x16)`: libsnd's own voices are 0–21. Effects key on voices 0–23 explicitly and choose free ones by key status, so they avoid voices the music holds.
- `pc-port-audio` provides: the SPU core with 24 voices and a mixer called from the SDL2 callback; `libsnd.c` with the VAB table (programs, 16 tones each with `min`/`max`/`center`/`shift`/ADSR/`vol`/`pan`), `key_on(voice, vab, prog, tone, note, fine, voll, volr)`, and voice release. Core calls take the SDL audio device lock, which SDL documents as recursive.

## Goals / Non-Goals

**Goals:**

- The game's sequences play with their notes, programs, tempo, volume, pan, and loops.
- Timing on the audio clock, exact to the sample, so music never drifts with frame rate.
- No allocation in the callback; sequence storage is static.

**Non-Goals:**

- Pitch bend, modulation, portamento, SysEx, reverb, SEP files, and CD-XA.
- Reproducing the PS1's 60 Hz tick quantization; sample-accurate timing is finer than the original.

## Decisions

### 1. SEQ layout and storage

```
offset 0   "pQES"                       magic
offset 4   version     u32 big-endian
offset 8   resolution  u16 big-endian   ticks per quarter note
offset 10  tempo       u24 big-endian   microseconds per quarter note
offset 13  rhythm      u8 numerator, u8 denominator (log2)
offset 15  event stream (MIDI events, each after a variable-length delta time)
```

`SsSeqOpen` scans the stream for `FF 2F 00` (End of Track) to find its length, refuses streams longer than a slot, and copies the header fields and the stream into one of 8 static slots of 64 KB (512 KB total). The copy keeps the audio thread off game memory: the game reuses its sound buffer when it loads the next scene's banks. The handle is the slot index. The VAB id is stored with it.

### 2. The sequencer state

Per open sequence:

```
pos, loop_pos, loop_count, playing, paused
running_status
tempo_us, resolution
samples_to_next   (double)  samples until the next event is due
vol_l, vol_r      (0..127 from SsSeqSetVol)
channel[16]: program, volume (CC 7, default 127), pan (CC 10, default 64)
held[24]:   which (channel, note) each voice is playing for this sequence
```

Reading an event: a variable-length delta (7 bits per byte, high bit continues), then a status byte, or running status if the byte is below `0x80`. Lengths: `8x`/`9x`/`Ax`/`Bx`/`Ex` take 2 data bytes, `Cx`/`Dx` take 1, `FF type len data` and `F0`/`F7 len data` are skipped by length except Tempo (`FF 51 03`) and End of Track (`FF 2F 00`).

| Event | Action |
| --- | --- |
| `9n note vel` (vel > 0) | key on (section 3) |
| `8n note vel`, `9n note 0` | release the voices held for (n, note) |
| `Cn prog` | channel program |
| `Bn 07 v` / `Bn 0A p` | channel volume / pan |
| `Bn 63 14` | loop start: `loop_pos` = next event, count from the next `Bn 06 c` (0 = forever) |
| `Bn 63 1E` | loop end: jump to `loop_pos` while count is 0 or remaining |
| `FF 51 03 t` | tempo |
| `FF 2F 00` | play count 0: rewind to the first event; otherwise stop |

When a delta is read, `samples_to_next += delta × tempo_us × 44100 / (resolution × 1,000,000)`. Keeping the remainder in a double means rounding never accumulates.

### 3. Notes on the voice path

For a Note On on channel `n`: the sequence's VAB, the channel's program, and for each tone `t` of that program (`0 .. prog.tones − 1`) with `t.min <= note <= t.max`:

- voice: the lowest voice below the reserved count with key status off or ended; otherwise the one started earliest in that range;
- volumes: `velocity/127 × channel.volume/127 × vol_l/127` (and right), with the channel pan folded into the tone and program pan exactly as `key_on` does;
- `key_on(voice, vab, prog, t, note, 0, voll, volr)` from `pc-port-audio`;
- record (n, note) in `held[voice]`.

Note Off releases every voice whose `held` matches and clears it. `SsSeqStop`, `SsSeqPause`, and `SsSeqClose` release all voices held by that sequence. A voice that an effect takes over (key status reused) is dropped from `held` when the effect keys it on.

### 4. Running on the audio clock

`spu_core.c`'s callback becomes:

```
while frames remain:
    n = min(frames remaining, 4096, seq_frames_until_next_event())
    if n == 0: seq_run_due_events(); continue
    spu_mix(out, n); seq_advance(n); out += n
```

`seq_frames_until_next_event` is the smallest `ceil(samples_to_next)` over playing sequences. `seq_advance(n)` subtracts `n` from each. `seq_run_due_events` handles every event whose `samples_to_next <= 0`, in stream order, with a cap of 4096 events per call to survive a stream of zero-delta events. Notes therefore start on the exact sample their tick falls on.

These run on the audio thread while SDL holds the device lock; the voice calls they make take the same recursive lock. Game-thread calls (`SsSeqPlay`, `SsSeqSetVol`, …) take the lock, so the callback never sees a half-updated sequence.

### 5. Diagnostics

With `HMBTN_AUDIO_DUMP` set, `SsSeqOpen` logs the handle, VAB, resolution, tempo, and stream length; `SsSeqPlay`/`Stop` log the handle. Note events are not logged.

## Risks / Trade-offs

- [A sequence uses events this design skips] → Unknown controllers and pitch bend are ignored; a melody stays in tune but loses bends.
- [Voice stealing between music and effects] → As on the PS1: effects choose free voices by key status, and music steals the oldest voice when none is free.
- [Slot size] → 64 KB per sequence; a larger stream is refused with a log line rather than truncated.
- [Recursive lock reliance] → SDL2 mutexes are recursive on all platforms SDL2 supports; the callback already holds the device lock when it calls the core.

## Migration Plan

1. `seq_player.c`: open/close, header, stream copy, and offline checks.
2. Event reading, timing, notes, and loops, checked by stepping a synthetic SEQ by sample counts with the device paused.
3. The callback split and the game-facing SsSeq calls.
4. The title run, then a long run for loops and memory.
5. Roll back by restoring the SsSeq no-ops; effects keep working.

## Open Questions

None.
