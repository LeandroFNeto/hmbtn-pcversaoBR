# Tasks

## 1. The SPU core

- [x] 1.1 Add `src/pc_port/include/spu_core.h` and `src/pc_port/src/spu_core.c` with the static 512 KB sound RAM and `spu_ram_write` clamped at the end. Verify `HMBTN_SELFTEST=1` checks: 32 bytes written at `0x1010` read back equal; a write starting 16 bytes before the end writes only 16 bytes.
- [x] 1.2 Add the ADPCM block decoder and the per-voice block walk with loop flags per design.md section 4. Verify checks: shift 12 / filter 0 nibbles 1, 7, 8, 15 decode to 1, 7, −8, −1; a one-shot sample (last block flags `0x01`) ends its voice; a looping sample (flags `0x04` … `0x03`) is still sounding after three times its length.
- [x] 1.3 Add the ADSR envelope and key status per design.md section 4. Verify checks: a voice with a fast attack reaches a non-zero envelope within 100 samples and reports status 1; after release it reaches zero and reports 0; an ended one-shot that was not keyed off reports 3.
- [x] 1.4 Add pitch stepping with linear interpolation, volumes, and `spu_mix`. Verify checks: pitch 8192 consumes two source samples per output sample; a voice at volume 0 adds nothing; two voices sum and clamp to 16 bits.

## 2. The device

- [x] 2.1 Add `spu_audio_open` / `spu_audio_close` with the SDL2 device, callback, and the lock around every game-thread state change per design.md section 5; call them from `main_pc.c` after `SDL_Init` and in `teardown`. Verify the MinGW build links, the log shows the device's obtained format (44,100 Hz, S16, 2 channels) or the no-device line, and closing during `VSync` exits with code 0 after an `audio closed` log line.
- [x] 2.2 Add the `HMBTN_AUDIO_DUMP` WAV writer per design.md section 6. Verify a run with a self-check tone keyed on writes a WAV whose header says 44,100 Hz stereo 16-bit and whose samples are non-zero where the tone played.

## 3. libsnd and libspu

- [x] 3.1 Rewrite `libsnd.c`: `SsVabOpenHead` per design.md section 2 with the first-fit allocator, `SsVabTransBodyPartly`, `SsVabTransCompleted`, and `SsVabClose`. Verify checks with a synthetic VAB (2 programs, 3 VAGs): open returns 0, a bad magic returns −1, two body chunks land back to back at the VAB base, and closing then reopening reuses the same sound RAM.
- [x] 3.2 Add `SsUtKeyOnV`, `SsUtKeyOn`, `SsUtKeyOffV`, `SsUtKeyOff`, `SsVoKeyOff`, `SsUtAllKeyOff`, `SsUtSetVVol`, `SsSetMVol`, and `SsSetReservedVoice` per design.md section 3; keep reverb, stereo/mono, and SEQ calls as accepted no-ops. Verify checks on the synthetic VAB: key on at the center note gives pitch 4096; one octave up gives 8192; a missing VAB returns −1; `SsUtKeyOn` picks the free voice below the reserved count.
- [x] 3.3 Rewrite `libspu.c`: `SpuInit`, `SpuSetCommonAttr` (master volume mask), `SpuGetKeyStatus`, `SpuGetAllKeysStatus`, `SpuSetTransferStartAddr`, `SpuWrite`, and `SpuSetKey` on the core. Verify checks: status bytes follow design.md section 4 for an off, on, released, and ended voice; `SpuWrite` after `SpuSetTransferStartAddr(0x2000)` lands at `0x2000`.

## 4. The title and memory

- [x] 4.1 With `HMBTN_RUN_GAME=ON`, the extracted disc, and `HMBTN_AUDIO_DUMP` set, run the title input script: wait for the title, press Enter, wait 2 s, close. Verify the log shows VABs opened and a key-on after the press, and the WAV is silent before the press and has a peak above −30 dBFS within 0.5 s after it. Embed the waveform summary in the change notes. If nothing sounds, record the VAB opens, the key-on arguments, and the voice status, and pause.
- [x] 4.2 Run the game for one minute pressing Enter and `X` to reach dialogue, and compare private bytes at 10 s and 60 s. Verify they differ by less than 1 MB, closing exits with code 0, and `git diff -- src/main src/overlay src/include` is empty.

## Workflow follow-up

- Archive `pc-port-audio` after review, once these tasks are checked.
- SEQ music and XA streaming are separate changes on this core.
