# host-libsnd-sfx Specification

## Purpose
Serves the game's `libsnd` sound-effect calls and `libspu` status calls on the host SPU, so the sound banks it loads play the effects it keys on.

## Requirements

### Requirement: SsVabOpenHead reads a VAB header
`SsVabOpenHead(addr, -1)` SHALL check the `pBAV` magic, read the program count, the 128 program attributes, 16 tone attributes per program, and the 256-entry VAG size table, reserve sound RAM for the body, and return a VAB id from 0 to 15. A header without the magic, or with no free id or room, SHALL return −1.

#### Scenario: Valid header
- **WHEN** the game opens a VAB header with the `pBAV` magic
- **THEN** the call returns an id from 0 to 15

#### Scenario: Not a VAB
- **WHEN** the header's first word is not the `pBAV` magic
- **THEN** the call returns −1

### Requirement: The body transfers in chunks
`SsVabTransBodyPartly(addr, size, id)` SHALL copy `size` bytes after the previous chunk of that VAB into its reserved sound RAM, and SHALL return the id. `SsVabTransCompleted` SHALL report the transfer complete. `SsVabClose(id)` SHALL free the id and its sound RAM.

#### Scenario: Two chunks
- **WHEN** a body is sent in two chunks
- **THEN** sound RAM holds the two chunks back to back from the VAB's base

### Requirement: SsUtKeyOnV plays a tone
`SsUtKeyOnV(voice, vab, prog, tone, note, fine, voll, volr)` SHALL start `voice` on the VAG of that program's tone, at a pitch of 4096 × 2^(((note × 128 + fine) − (center × 128 + shift)) / 1536) from the tone's center and shift, with the tone's ADSR, and with left and right volumes from `voll`, `volr`, the tone, program, and VAB volumes and pans. It SHALL return the voice, or −1 when the VAB, program, or tone does not exist.

#### Scenario: Confirmation sound
- **WHEN** the game keys on a loaded tone at its center note
- **THEN** the voice plays its VAG at pitch 4096 and reports key status on

#### Scenario: Missing VAB
- **WHEN** the game keys on a VAB id that is not open
- **THEN** the call returns −1 and no voice starts

### Requirement: SsUtKeyOn chooses a voice
`SsUtKeyOn` SHALL start the tone on the lowest voice below the count set by `SsSetReservedVoice` (24 by default) that is off, or else on the one that started earliest in that range, and return it.

#### Scenario: Free voice
- **WHEN** every voice in the range but one is sounding
- **THEN** the tone starts on the silent voice

### Requirement: Key off and volume calls
`SsUtKeyOffV(voice)`, `SsVoKeyOff(voice + (vab << 8), mode)`, and `SsUtKeyOff(voice, …)` SHALL put the voice into release. `SsUtAllKeyOff` SHALL release every voice. `SsUtSetVVol(voice, l, r)` SHALL set that voice's volumes. `SsSetMVol(l, r)` and `SpuSetCommonAttr` with the master-volume mask SHALL set the master volume.

#### Scenario: Stop a voice
- **WHEN** the game calls `SsUtSetVVol(voice, 0, 0)` and `SsUtKeyOffV(voice)`
- **THEN** the voice is silent at once and reports off once its release ends

### Requirement: Key status uses the SDK's values
`SpuGetAllKeysStatus(status)` SHALL fill 24 bytes with 0 for an off voice, 1 for a keyed-on voice, 2 for a keyed-off voice still sounding, and 3 for a keyed-on voice whose envelope has ended. `SpuGetKeyStatus(mask)` SHALL report the lowest voice in `mask` the same way.

#### Scenario: Free voice search
- **WHEN** a one-shot sample on voice 5 has finished
- **THEN** `SpuGetAllKeysStatus` reports 0 or 3 for voice 5, so `sound_key_on_free_voice` can reuse it

### Requirement: Start sounds on the title
With the game running from the extracted disc and an audio device, pressing the Start key on the title SHALL key on a voice and produce non-silent output within half a second.

#### Scenario: Title confirmation
- **WHEN** the title waits and `Enter` is pressed with `HMBTN_AUDIO_DUMP` set
- **THEN** the trace shows a sound-effect key-on within half a second after the press
- **AND** the dump holds sound in that half second
