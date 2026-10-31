#ifndef PC_PORT_SPU_CORE_H
#define PC_PORT_SPU_CORE_H

#include <stdint.h>

/* The host SPU: 512 KB of sound RAM, 24 ADPCM voices with the PS1 ADSR envelope, and a
 * 44,100 Hz mixer played through an SDL2 audio device. Game-thread calls take the audio
 * device lock; the callback only mixes. */

#define SPU_RAM_SIZE (512 * 1024)
#define SPU_VOICES 24
#define SPU_RATE 44100
#define SPU_PITCH_ONE 4096

#define SPU_STATUS_OFF 0
#define SPU_STATUS_ON 1
#define SPU_STATUS_RELEASED 2
#define SPU_STATUS_ENDED 3

/* Device lifetime: open after SDL_Init, close before SDL_Quit (both exit paths). */
void spu_audio_open(void);
void spu_audio_close(void);
int spu_audio_is_open(void);
/* Stops and restarts the callback, for checks that mix by hand. */
void spu_audio_pause(int paused);

/* Sound RAM. Writes past the end are cut at SPU_RAM_SIZE. Returns the bytes written. */
uint32_t spu_ram_write(uint32_t addr, const void *data, uint32_t size);
void spu_ram_read(uint32_t addr, void *out, uint32_t size);

/* Voices. Volumes are 0..0x3FFF; pitch 4096 plays at 44,100 Hz. */
void spu_voice_start(int voice, uint32_t addr, uint32_t pitch, uint16_t adsr1, uint16_t adsr2, int vol_l, int vol_r);
void spu_voice_release(int voice);
void spu_voice_set_volume(int voice, int vol_l, int vol_r);
int spu_voice_status(int voice);
uint32_t spu_voice_start_order(int voice);
void spu_set_master(int vol_l, int vol_r);
void spu_reset_voices(void);

/* Mixes `frames` stereo frames into `out` (interleaved int16). Called by the audio
 * callback; checks call it directly with the device paused. */
void spu_mix(int16_t *out, int frames);
/* Mixes `frames` frames with the sequencer: splits at each due event, runs it, and mixes
 * on. The audio callback uses this; checks call it with the device paused. */
void spu_render(int16_t *out, int frames);
/* The audio device lock (recursive), for state the callback also reads. */
void spu_lock(void);
void spu_unlock(void);

/* Pure parts and probes, for checks. */
void spu_decode_block(const uint8_t *block, int16_t out[28], int16_t *s1, int16_t *s2);
uint32_t spu_debug_samples_consumed(int voice);
/* With HMBTN_AUDIO_DUMP set: trace on, and the dump position in frames for log lines. */
int spu_debug_tracing(void);
uint32_t spu_debug_dump_frames(void);
int spu_debug_envelope(int voice);
uint32_t spu_debug_pitch(int voice);

#endif
