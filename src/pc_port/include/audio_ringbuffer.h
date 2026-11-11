#ifndef AUDIO_RINGBUFFER_H
#define AUDIO_RINGBUFFER_H

#include <SDL.h>
#include <stdint.h>

#define AUDIO_RING_FRAMES 16384        /* 16,384 stereo frames (64 KB PCM S16) */
#define AUDIO_RING_MASK   (AUDIO_RING_FRAMES - 1)
#define AUDIO_CHUNK_BATCH 512          /* Batch size for generation */

void audio_ringbuffer_init(void);
void audio_ringbuffer_clear(void);

/* Single-producer write: copies stereo frames into the ring buffer.
 * Returns number of frames actually written. */
int audio_ringbuffer_write(const int16_t *in, int frames);

/* Single-consumer read: copies stereo frames from the ring buffer into out.
 * If buffer starves, fills missing frames with silence (0) and increments underrun count.
 * Returns number of frames read from buffer (excluding padded silence). */
int audio_ringbuffer_read(int16_t *out, int frames);

/* Space queries */
int audio_ringbuffer_available_read(void);
int audio_ringbuffer_available_write(void);
int audio_ringbuffer_underruns(void);

#endif /* AUDIO_RINGBUFFER_H */
