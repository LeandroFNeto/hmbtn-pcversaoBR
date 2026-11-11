/* Static lock-free Single-Producer Single-Consumer (SPSC) Audio Ring Buffer in BSS. */

#include <audio_ringbuffer.h>
#include <string.h>

static int16_t ring_buffer[AUDIO_RING_FRAMES * 2];
static SDL_atomic_t ring_write_pos;
static SDL_atomic_t ring_read_pos;
static SDL_atomic_t ring_underrun_count;

void audio_ringbuffer_init(void) {
    memset(ring_buffer, 0, sizeof(ring_buffer));
    SDL_AtomicSet(&ring_write_pos, 0);
    SDL_AtomicSet(&ring_read_pos, 0);
    SDL_AtomicSet(&ring_underrun_count, 0);
}

void audio_ringbuffer_clear(void) {
    memset(ring_buffer, 0, sizeof(ring_buffer));
    SDL_AtomicSet(&ring_write_pos, 0);
    SDL_AtomicSet(&ring_read_pos, 0);
    SDL_AtomicSet(&ring_underrun_count, 0);
}

int audio_ringbuffer_available_read(void) {
    int w = SDL_AtomicGet(&ring_write_pos);
    int r = SDL_AtomicGet(&ring_read_pos);
    int diff = w - r;
    return diff > 0 ? diff : 0;
}

int audio_ringbuffer_available_write(void) {
    int w = SDL_AtomicGet(&ring_write_pos);
    int r = SDL_AtomicGet(&ring_read_pos);
    int diff = w - r;
    int free_space = AUDIO_RING_FRAMES - diff;
    return free_space > 0 ? free_space : 0;
}

int audio_ringbuffer_underruns(void) {
    return SDL_AtomicGet(&ring_underrun_count);
}

int audio_ringbuffer_write(const int16_t *in, int frames) {
    int w;
    int r;
    int free_space;
    int to_write;
    int idx;
    int first;

    if (in == NULL || frames <= 0) {
        return 0;
    }
    w = SDL_AtomicGet(&ring_write_pos);
    r = SDL_AtomicGet(&ring_read_pos);
    free_space = AUDIO_RING_FRAMES - (w - r);
    if (free_space <= 0) {
        return 0;
    }
    to_write = frames < free_space ? frames : free_space;
    idx = w & AUDIO_RING_MASK;
    if (idx + to_write <= AUDIO_RING_FRAMES) {
        memcpy(&ring_buffer[idx * 2], in, (size_t)to_write * 4);
    } else {
        first = AUDIO_RING_FRAMES - idx;
        memcpy(&ring_buffer[idx * 2], in, (size_t)first * 4);
        memcpy(&ring_buffer[0], in + first * 2, (size_t)(to_write - first) * 4);
    }
    SDL_AtomicSet(&ring_write_pos, w + to_write);
    return to_write;
}

int audio_ringbuffer_read(int16_t *out, int frames) {
    int w;
    int r;
    int ready;
    int to_read;
    int idx;
    int first;
    int missing;

    if (out == NULL || frames <= 0) {
        return 0;
    }
    w = SDL_AtomicGet(&ring_write_pos);
    r = SDL_AtomicGet(&ring_read_pos);
    ready = w - r;
    if (ready <= 0) {
        /* Starvation: fill entire block with silence */
        memset(out, 0, (size_t)frames * 4);
        SDL_AtomicAdd(&ring_underrun_count, 1);
        return 0;
    }

    to_read = frames < ready ? frames : ready;
    idx = r & AUDIO_RING_MASK;
    if (idx + to_read <= AUDIO_RING_FRAMES) {
        memcpy(out, &ring_buffer[idx * 2], (size_t)to_read * 4);
    } else {
        first = AUDIO_RING_FRAMES - idx;
        memcpy(out, &ring_buffer[idx * 2], (size_t)first * 4);
        memcpy(out + first * 2, &ring_buffer[0], (size_t)(to_read - first) * 4);
    }
    SDL_AtomicSet(&ring_read_pos, r + to_read);

    missing = frames - to_read;
    if (missing > 0) {
        /* Partial underrun: fill remainder with silence */
        memset(out + to_read * 2, 0, (size_t)missing * 4);
        SDL_AtomicAdd(&ring_underrun_count, 1);
    }

    return to_read;
}
