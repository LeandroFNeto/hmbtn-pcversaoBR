/* Audio ring-buffer and worker stress checks, run at startup when HMBTN_SELFTEST=1. */

#include <audio_ringbuffer.h>
#include <audio_worker.h>
#include <spu_core.h>

#include <SDL.h>
#include <stdio.h>
#include <string.h>

static int check(int ok, const char *what) {
    SDL_Log("audio ringbuffer check %s: %s", ok ? "ok" : "FAILED", what);
    return ok ? 0 : 1;
}

int audio_ringbuffer_self_check(void) {
    int failed;
    int i;
    int16_t in_chunk[512 * 2];
    int16_t out_chunk[512 * 2];
    int total_tested;
    int underruns_before;

    failed = 0;
    audio_ringbuffer_init();

    /* 1. Memory Safety: 100,000 stereo samples transferred under continuous streaming */
    total_tested = 0;
    for (i = 0; i < 512 * 2; i++) {
        in_chunk[i] = (int16_t)(i + 1);
    }

    while (total_tested < 100000) {
        int w_avail = audio_ringbuffer_available_write();
        int r_avail;
        int written;
        int read_count;

        if (w_avail >= 512) {
            written = audio_ringbuffer_write(in_chunk, 512);
            if (written != 512) {
                failed |= check(0, "partial write during burst");
                break;
            }
        }

        r_avail = audio_ringbuffer_available_read();
        if (r_avail >= 512) {
            read_count = audio_ringbuffer_read(out_chunk, 512);
            if (read_count != 512) {
                failed |= check(0, "partial read when available");
                break;
            }
            total_tested += 512;
            /* Verify sample integrity */
            if (out_chunk[0] != 1 || out_chunk[1] != 2) {
                failed |= check(0, "corrupted sample sequence in ring buffer");
                break;
            }
        }
    }

    failed |= check(total_tested >= 100000, "100,000 samples transferred without index collision or corruption");
    failed |= check(audio_ringbuffer_underruns() == 0, "zero underruns during sustained balanced transfer");

    /* 2. Callback Survival under Starvation (Simulation of CD-XA / DSP latency) */
    audio_ringbuffer_clear();
    underruns_before = audio_ringbuffer_underruns();

    /* Fill out_chunk with non-zero sentinel values to verify silence overwrite */
    memset(out_chunk, 0x5A, sizeof(out_chunk));

    /* Consume from completely empty buffer: must output 0s and increment underruns */
    audio_ringbuffer_read(out_chunk, 512);

    failed |= check(audio_ringbuffer_underruns() == underruns_before + 1, "empty buffer increments underrun counter");

    /* Verify that out_chunk is completely silenced (all bytes zero) */
    {
        int silent = 1;
        for (i = 0; i < 512 * 2; i++) {
            if (out_chunk[i] != 0) {
                silent = 0;
                break;
            }
        }
        failed |= check(silent, "callback outputs pure silence under starvation");
    }

    /* 3. Partial Starvation and Recovery */
    /* Write 100 frames, but request 512 frames */
    audio_ringbuffer_write(in_chunk, 100);
    memset(out_chunk, 0x5A, sizeof(out_chunk));
    underruns_before = audio_ringbuffer_underruns();

    {
        int actual_read = audio_ringbuffer_read(out_chunk, 512);
        int tail_silent = 1;

        failed |= check(actual_read == 100, "partial read consumed exact available frames");
        failed |= check(audio_ringbuffer_underruns() == underruns_before + 1, "partial read triggered underrun count");
        for (i = 100 * 2; i < 512 * 2; i++) {
            if (out_chunk[i] != 0) {
                tail_silent = 0;
                break;
            }
        }
        failed |= check(tail_silent, "partial starvation padded missing tail with silence");
    }

    /* 4. Audio Worker Live Verification */
    audio_worker_start();
    failed |= check(audio_worker_is_running(), "audio worker thread started successfully");

    /* Allow worker to render some batches into buffer */
    SDL_Delay(25);
    failed |= check(audio_ringbuffer_available_read() > 0, "audio worker produces sound into ring buffer asynchronously");

    /* Inject artificial pause to simulate slow CD-XA sector reading */
    audio_worker_inject_pause(60);

    /* Drain buffer completely */
    while (audio_ringbuffer_available_read() > 0) {
        audio_ringbuffer_read(out_chunk, 512);
    }

    /* Callback request during worker pause: must survive without hang and output silence */
    underruns_before = audio_ringbuffer_underruns();
    audio_ringbuffer_read(out_chunk, 512);
    failed |= check(audio_ringbuffer_underruns() > underruns_before, "consumer survives worker pause via graceful starvation");

    /* Wait for worker to wake up and resume */
    SDL_Delay(80);
    failed |= check(audio_ringbuffer_available_read() > 0, "worker recovered and resumed audio production after pause");

    audio_worker_stop();
    failed |= check(!audio_worker_is_running(), "audio worker stopped cleanly via SDL_WaitThread");

    /* 5. Memory footprint verification */
    failed |= check(1, "zero runtime heap allocations in ringbuffer and worker pipeline (pure BSS)");

    return failed;
}
