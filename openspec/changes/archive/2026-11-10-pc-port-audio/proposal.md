# Proposal

## Why

The host runs the game from the title into the new-game introduction, in silence. Every sound call lands in Phase 1 stubs: `SsVabOpenHead` and `SsVabTransBodyPartly` drop the sound banks, `SsUtKeyOnV` plays nothing, and `SpuGetAllKeysStatus` reports every voice off. The title's Start confirmation (`sound_play(SOUND_MENU_CONFIRM)`) is the first sound a player expects.

## What Changes

- A host SPU: 512 KB of sound RAM, 24 voices that decode PS1 ADPCM (VAG) blocks with their loop flags, the PS1 ADSR envelope, per-voice pitch and stereo volume, and a mixer at 44,100 Hz.
- An SDL2 audio device (`SDL_InitSubSystem(SDL_INIT_AUDIO)`, `SDL_OpenAudioDevice`, 44,100 Hz, `AUDIO_S16SYS`, stereo) whose callback mixes the active voices. The game thread changes voice state only under `SDL_LockAudioDevice`. The callback uses static buffers only.
- `libsnd` sound effects as the game uses them: `SsVabOpenHead` parses a VAB header (programs, tones, VAG sizes), `SsVabTransBodyPartly` copies the body into sound RAM, `SsVabClose` frees it, `SsUtKeyOnV`/`SsUtKeyOn` start a tone at its note and volume, `SsUtKeyOffV`/`SsUtKeyOff`/`SsVoKeyOff`/`SsUtAllKeyOff` release voices, `SsUtSetVVol` and `SsSetMVol` set volumes, and `SsSetReservedVoice` limits automatic voice choice.
- `libspu` as the game uses it: `SpuSetCommonAttr` (master volume), `SpuGetKeyStatus`, and `SpuGetAllKeysStatus`, with the SDK's status values, so `sound_key_on_free_voice` finds free voices. `SpuInit`, `SpuSetTransferStartAddr`, `SpuWrite`, and `SpuSetKey` are added on the same core, since the VAB transfer is a sound-RAM write.
- Correction to the request: the game does not call `SpuWrite`, `SpuSetKey`, or `SpuSetVoiceAttr` itself; it plays sound effects through `libsnd`'s VAB and `SsUt*` calls. Those are what the host implements; `SpuSetVoiceAttr` stays out.
- A diagnostic: with `HMBTN_AUDIO_DUMP=<file.wav>`, the host also records its first 20 seconds of mixed output and writes them as a WAV file at exit.
- No change under `src/`.

## Capabilities

### New Capabilities

- `host-spu`: The host's sound RAM, ADPCM voices, envelope, mixer, and SDL2 output device.
- `host-libsnd-sfx`: The host serves `libsnd`'s VAB loading and sound-effect voice calls, and `libspu`'s status and master-volume calls, on the host SPU.

### Modified Capabilities

- None.

## Impact

- `src/pc_port/src/libsnd.c` and `libspu.c` (rewritten), new `src/pc_port/src/spu_core.c` and `src/pc_port/include/spu_core.h`, `main_pc.c` (audio init and shutdown), a new check file.
- No new package: audio is part of SDL2.
- Out of scope: SEQ music (`SsSeqOpen`, `SsSeqPlay`, …, so the title stays without music), XA streaming from CD, reverb (accepted, not applied), pitch modulation, noise, and Gaussian interpolation (linear interpolation is used).
