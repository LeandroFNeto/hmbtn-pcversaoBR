/* Audio Worker thread for asynchronous SPU mixing and sequencing. */

#include <audio_worker.h>
#include <audio_ringbuffer.h>
#include <spu_core.h>

#include <SDL.h>

static SDL_Thread *worker_thread = NULL;
static SDL_atomic_t worker_running;
static SDL_atomic_t worker_paused;
static SDL_atomic_t test_pause_ms;

static int SDLCALL audio_worker_thread(void *data) {
    int16_t batch[AUDIO_CHUNK_BATCH * 2];
    int pause;
    int free_frames;

    (void)data;
    while (SDL_AtomicGet(&worker_running)) {
        if (SDL_AtomicGet(&worker_paused)) {
            SDL_Delay(5);
            continue;
        }

        pause = SDL_AtomicSet(&test_pause_ms, 0);
        if (pause > 0) {
            SDL_Delay((Uint32)pause);
        }

        free_frames = audio_ringbuffer_available_write();
        if (free_frames >= AUDIO_CHUNK_BATCH) {
            spu_lock();
            spu_render(batch, AUDIO_CHUNK_BATCH);
            spu_unlock();
            audio_ringbuffer_write(batch, AUDIO_CHUNK_BATCH);
        }

        /* Adaptive sleep when buffer is adequately filled */
        if (audio_ringbuffer_available_read() >= (AUDIO_RING_FRAMES * 3 / 4) ||
            free_frames < AUDIO_CHUNK_BATCH) {
            SDL_Delay(2);
        }
    }
    return 0;
}

void audio_worker_start(void) {
    if (worker_thread != NULL) {
        return;
    }
    SDL_AtomicSet(&worker_running, 1);
    SDL_AtomicSet(&worker_paused, 0);
    SDL_AtomicSet(&test_pause_ms, 0);
    worker_thread = SDL_CreateThread(audio_worker_thread, "HMBTN_AudioWorker", NULL);
}

void audio_worker_stop(void) {
    if (worker_thread == NULL) {
        return;
    }
    SDL_AtomicSet(&worker_running, 0);
    SDL_WaitThread(worker_thread, NULL);
    worker_thread = NULL;
}

void audio_worker_pause(int paused) {
    SDL_AtomicSet(&worker_paused, paused);
}

int audio_worker_is_running(void) {
    return worker_thread != NULL && SDL_AtomicGet(&worker_running);
}

void audio_worker_inject_pause(int ms) {
    SDL_AtomicSet(&test_pause_ms, ms);
}
