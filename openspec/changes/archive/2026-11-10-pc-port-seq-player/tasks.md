# Tasks

## 1. Opening sequences

- [x] 1.1 Add `src/pc_port/include/seq_player.h` and `src/pc_port/src/seq_player.c` with 8 static 64 KB slots, header parsing, the End of Track scan, `SsSeqOpen`, and `SsSeqClose` per design.md section 1 (moving them out of `libsnd.c`). Verify `HMBTN_SELFTEST=1` checks with a synthetic SEQ: a valid stream returns handle 0 with resolution and tempo read; a stream without `pQES` returns −1; a stream without End of Track returns −1; closing frees the slot for the next open.

## 2. The sequencer

- [x] 2.1 Add event reading with variable-length deltas, running status, and the event table in design.md section 2. Verify checks that step a synthetic SEQ by sample counts with the device paused: three running-status notes start three voices; a pitch bend between two notes is skipped and both notes start; a Program Change switches the tone used by the next note.
- [x] 2.2 Add sample timing. Verify checks: at resolution 480 and tempo 500,000 a Note On at tick 480 has not started after 22,049 samples and has started after 22,050; after a Tempo event to 250,000 the next 480 ticks take 11,025 samples.
- [x] 2.3 Add notes on the voice path per design.md section 3. Verify checks: a note's voice reports status 1 until its Note Off and 2 after it; with every voice below the reserved count busy, the next note takes the earliest-started one; Control Change 7 at 0 makes the next note silent in the mix.
- [x] 2.4 Add loops: loop markers and End of Track restart for play count 0. Verify checks: a sequence with `Bn 63 14` / `Bn 06 00` / `Bn 63 1E` starts its loop's first note again after the loop end; a sequence without markers played with count 0 starts its first note again after its End of Track.

## 3. Integration

- [x] 3.1 Split the audio callback at event boundaries per design.md section 4, and implement `SsSeqPlay`, `SsSeqStop`, `SsSeqPause`, `SsSeqReplay`, and `SsSeqSetVol` under the audio lock. Verify checks: stop releases every voice the sequence held; pause then replay continues with the next event rather than the first; the MinGW build has no warnings in the new files.
- [x] 3.2 Reword `pc-port-audio`'s `host-libsnd-sfx` title scenario so it checks the confirmation key-on in the trace and sound within half a second of the press, not silence before it. Verify `openspec validate --changes --strict` passes.

## 4. The game

- [x] 4.1 With `HMBTN_RUN_GAME=ON`, the extracted disc, and `HMBTN_AUDIO_DUMP` set, run `build/host/title_audio.ps1`. Verify the log shows the title sequence opened (resolution, tempo) and played, the WAV has sound in the seconds before the Enter press, and the confirmation key-on still follows the press. Embed the waveform summary. If the title is silent, record the open/play log lines and the first events read, and pause.
- [x] 4.2 Run the title for 20 seconds without input with the dump on, and verify the music is still sounding in the last two seconds of the dump (it looped or is still playing). Then run one minute with input into the introduction and verify private bytes at 10 s and 60 s differ by less than 1 MB, closing exits with code 0, and `git diff -- src/main src/overlay src/include` is empty.

Notes from the runs (2026-10-31): the title opens SEQ 0 (VAB 2, resolution 480, tempo 480,000 µs, 10,174 bytes) and plays it with count 0; music sounds from 3.5 s, is still sounding at 19.75 s of a 20 s dump, and 142 of 1,455,300 music samples (0.0098%) hit the 16-bit clamp at an RMS of −16.2 dBFS. The confirmation key-on after Enter landed on voice 4, past the four voices the music held. Two `pc-port-audio` defects were fixed on the way: the decay phase stepped once before comparing with the sustain level (a maximum sustain halved every tone after its attack), and `SsUtKeyOn`'s oldest-voice fallback always returned voice 0.

## Workflow follow-up

- Archive `pc-port-seq-player` after `pc-port-audio`, once these tasks are checked.
