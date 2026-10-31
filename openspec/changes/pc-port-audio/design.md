# Design

## Context

See proposal.md for why. How the game makes sound, from `src/main/sound/`:

- `sound_init` calls `SsInit`, `SsSetTableSize`, `SsSetTickMode(1)`, `SsSetReservedVoice(0x16)`, `SsUtReverbOff`, and `SpuSetCommonAttr` with master volume `0x3FFF` and CD volume `0x1FFF`.
- `sound_load_step` reads a sound bank from A_FILE member 9 (`objcg\sound.stm`). For a VAB it copies the header to RAM, calls `SsVabOpenHead(header, -1)`, then streams the body in `VAB_STREAM_CHUNK`-byte pieces with `SsVabTransBodyPartly(chunk, size, id)` and spins on `SsVabTransCompleted(1)`. SEQ entries are copied whole for `SsSeqOpen`.
- `sound_play(id)` → `sound_key_on_free_voice(id, 0, 23)`: `SpuGetAllKeysStatus(status)`, a voice whose status is 0 or 3 is free, then `SsUtKeyOnV(voice, def->vabId, def->prog, def->tone, def->note, def->fine, vol, vol)`.
- Other calls: `SsUtKeyOn` / `SsUtKeyOff` for the single "tone" voice, `SsUtKeyOffV`, `SsVoKeyOff(voice + (vab << 8), mode)`, `SsUtSetVVol`, `SsUtAllKeyOff`, `SsSetMVol(level, level)` from the master fader, reverb setters, `SsSetStereo`/`SsSetMono`, and `SpuGetKeyStatus` in minigame 3.
- The host runs the game on the main thread; SDL2 is already initialized for video, events, and game controllers. All host memory is static so far.

## Goals / Non-Goals

**Goals:**

- Sound effects as the PS1 plays them: the bank's own ADPCM data, pitch, envelope, volume, and pan.
- A mixer that never blocks the game and never allocates.
- Silence, not failure, on machines without an audio device.

**Non-Goals:**

- SEQ playback, XA streaming, reverb, pitch modulation, noise, and the SPU's Gaussian interpolation.
- Bit-exact output; the goal is the right sound, not a matching waveform.

## Decisions

### 1. One SPU core, two thin APIs on top

`spu_core.c` owns the hardware model:

```
static uint8_t  spu_ram[512 * 1024];
static SpuVoice voices[24];   /* address, repeat address, pitch, phase, decoded block,
                                 history s1/s2, ADSR words and state, volumes, key status */
static int      master_l, master_r;
```

Its functions are the SPU's operations: `spu_ram_write(addr, data, size)`, `spu_voice_start(v, addr, pitch, adsr1, adsr2, vl, vr)`, `spu_voice_release(v)`, `spu_voice_set_volume`, `spu_voice_status(v)`, `spu_set_master`, and `spu_mix(int16_t *out, int frames)`.

`libspu.c` and `libsnd.c` call only those. `SpuWrite` and `SpuSetTransferStartAddr` wrap `spu_ram_write`; `SpuSetKey` wraps start and release for voices whose attributes are already set. The game itself uses `libsnd` for every sound effect.

### 2. VAB parsing

The header layout, PSY-Q 4.x:

```
offset 0      VabHdr        32 bytes   form 'pBAV' (0x56414270), ver, id, fsize, reserved0,
                                       ps (programs), ts (tones), vs (VAGs), mvol, pan, attr1, attr2, reserved1
offset 32     ProgAtr[128]  16 bytes each
offset 2080   VagAtr[ps][16] 32 bytes each
then          uint16 vag_size[256]     size of VAG n in 8-byte units; entry 0 unused
```

`SsVabOpenHead` copies the header into a host table (16 slots × at most 128 programs × 16 tones, static), sums the VAG sizes, and reserves that many bytes of sound RAM with a first-fit allocator over 16 blocks, starting at `0x1010` (the PS1 reserves the area below it). Each VAG's sound-RAM address is the VAB base plus the sizes before it. `SsVabTransBodyPartly` copies each chunk to `base + bytes_sent` through `spu_ram_write`. `SsVabClose` frees the block and the slot.

### 3. Key on

`SsUtKeyOnV(voice, vab, prog, tone, note, fine, voll, volr)`:

```
t      = vab.tones[prog][tone]                  (VagAtr)
addr   = vab.vag_addr[t.vag]
pitch  = 4096 * 2^(((note*128 + fine) - (t.center*128 + t.shift)) / 1536), clamped to 0x3FFF
pan_l  = min(1, (127 - pan) / 64), pan_r = min(1, pan / 64), pan = combine(t.pan, prog.mpan)
vol_l  = 0x3FFF * voll/127 * t.vol/127 * prog.mvol/127 * vab.mvol/127 * pan_l    (same for right)
spu_voice_start(voice, addr, pitch, t.adsr1, t.adsr2, vol_l, vol_r)
```

`SsUtKeyOn` picks the voice (lowest off voice in `[0, reserved)`, where `SsSetReservedVoice(n)` gives libsnd the first `n` voices and defaults to 24; else the oldest started there), then does the same. The game reserves 22 and plays its effects with `SsUtKeyOnV` on explicit voices 0–23. `SsUtKeyOffV`, `SsVoKeyOff` (voice = low byte), and `SsUtKeyOff` call `spu_voice_release`. Invalid VAB, program, or tone returns −1.

### 4. Voice playback

Per output sample, for each active voice:

1. Advance `phase += pitch` (4096 = one source sample). For each whole source sample consumed, take the next decoded sample; when the 28 samples of a block are used, decode the next 16-byte block:

   ```
   shift  = b[0] & 0x0F (values above 12 treated as 9), filter = (b[0] >> 4) & 7 (clamped to 4)
   sample = ((int16_t)(nibble << 12) >> shift) + (s1 * f0[filter] + s2 * f1[filter] + 32) / 64
   f0 = {0, 60, 115, 98, 122},  f1 = {0, 0, -52, -55, -60},  clamp to int16
   ```

   Flag bit 2 stores the block address as the repeat address. After a block with bit 0: bit 1 jumps to the repeat address, otherwise the voice ends (envelope to zero, status off).
2. Interpolate linearly between the last two source samples by `phase & 4095`.
3. Apply the envelope, then the left and right volumes, and add into an `int32_t` accumulator.

The envelope follows the PS1 SPU (nocash PSX-SPX "SPU ADSR"): `adsr1` gives attack mode/shift/step, decay shift, and sustain level `(level + 1) * 0x800`; `adsr2` gives sustain mode/direction/shift/step and release mode/shift. Each phase uses the SPU's cycle-counter rate formula and exponential step scaling. Attack runs to `0x7FFF`, decay to the sustain level, sustain until key off, release to zero.

Key status for `SpuGetAllKeysStatus`: off = 0; keyed on and envelope > 0 = 1; keyed off and envelope > 0 = 2; keyed on and envelope 0 (ended one-shot) = 3.

### 5. The SDL2 device and locking

`spu_audio_open()` runs after `SDL_Init`: `SDL_InitSubSystem(SDL_INIT_AUDIO)`, then `SDL_OpenAudioDevice(NULL, 0, &want, &have, 0)` with 44,100 Hz, `AUDIO_S16SYS`, 2 channels, 512 frames, and the callback `spu_audio_callback(userdata, stream, len)`, then `SDL_PauseAudioDevice(dev, 0)`. If it fails, `dev = 0`, a log line is written, and voice calls still update state (status works, nothing is heard).

The callback calls `spu_mix` into the stream. It uses a static `int32_t` accumulator of 4096 frames and processes the stream in pieces of at most that size; it never allocates, logs, or blocks except on SDL's own device lock.

Every core function the game thread calls wraps its state change in `SDL_LockAudioDevice(dev)` / `SDL_UnlockAudioDevice(dev)` (no-ops when `dev == 0`). VAB body copies hold the lock only for each chunk's copy.

`spu_audio_close()` runs in `teardown` before `SDL_Quit`, on both the normal and the `longjmp` exit: `SDL_CloseAudioDevice(dev)` stops and joins the callback, then `SDL_QuitSubSystem(SDL_INIT_AUDIO)`.

### 6. The WAV dump

When `HMBTN_AUDIO_DUMP` names a file, the callback also copies its output into a static 20-second stereo buffer (3.5 MB) until it is full. `spu_audio_close` writes that buffer as a WAV file after the device is closed, so the file write never runs on the audio thread. The title test reads that file to find silence before the Enter press and sound after it.

## Risks / Trade-offs

- [Envelope details differ from hardware] → Sounds may decay slightly differently. The ADSR follows the documented rate table; the acceptance test is audible presence and timing.
- [Linear instead of Gaussian interpolation] → Slightly brighter high notes. Acceptable for effects.
- [Lock contention] → The game takes the lock for a few microseconds per key-on; the callback holds it for one 512-frame mix. No measurable stall at 60 Hz.
- [The title plays its sound through SEQ] → Then nothing would sound on Start. `sound_play(SOUND_MENU_CONFIRM)` in `title.c` uses `SsUtKeyOnV`, so this does not apply to the confirmation.
- [Sound RAM fragmentation across scene changes] → First fit over 16 blocks; banks are opened and closed in the same order per scene.

## Migration Plan

1. `spu_core.c` with sound RAM, ADPCM decode, loop flags, ADSR, and offline checks (no device needed).
2. The SDL2 device, callback, locking, shutdown, and the WAV dump.
3. `libsnd` VAB parsing and the `SsUt*` calls, `libspu` status and master volume, with checks on a synthetic VAB.
4. The title run with the dump; the one-minute memory run.
5. Roll back by restoring the stubs; the game runs silent as before.

## Open Questions

None.
