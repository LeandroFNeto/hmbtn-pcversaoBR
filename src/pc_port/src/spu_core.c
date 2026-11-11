/* The host SPU. See spu_core.h. */

#include <spu_core.h>
#include <seq_player.h>
#include <audio_ringbuffer.h>
#include <audio_worker.h>

#include <SDL.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { ENV_OFF, ENV_ATTACK, ENV_DECAY, ENV_SUSTAIN, ENV_RELEASE };

typedef struct {
    int active;
    int keyed_on;
    uint32_t addr;          /* block being played */
    uint32_t repeat;
    uint32_t pitch;
    uint32_t phase;         /* 12-bit fraction toward the next source sample */
    int16_t block[28];
    int block_pos;
    uint8_t block_flags;
    int16_t s1, s2;         /* ADPCM history */
    int16_t prev, cur;      /* interpolation pair */
    uint32_t consumed;
    uint16_t adsr1, adsr2;
    int env_phase;
    int32_t env_level;
    int32_t env_counter;
    int vol_l, vol_r;
    uint32_t order;
} SpuVoice;

static uint8_t spu_ram[SPU_RAM_SIZE];
static SpuVoice voices[SPU_VOICES];
static int master_l = 0x3FFF;
static int master_r = 0x3FFF;
static uint32_t start_counter;

static SDL_AudioDeviceID audio_dev;
static int32_t mix_acc[4096 * 2];

#define DUMP_SECONDS 16
static int16_t dump_buf[DUMP_SECONDS * SPU_RATE * 2];
static uint32_t dump_frames;
static int dump_enabled;
static char dump_path[512];

/* SDL's device lock is recursive: the callback already holds it when the sequencer
 * calls back into the voice functions. */
static void lock(void) {
    if (audio_dev != 0) {
        SDL_LockAudioDevice(audio_dev);
    }
}

static void unlock(void) {
    if (audio_dev != 0) {
        SDL_UnlockAudioDevice(audio_dev);
    }
}

void spu_lock(void) {
    lock();
}

void spu_unlock(void) {
    unlock();
}

/* ---- sound RAM ---- */

uint32_t spu_ram_write(uint32_t addr, const void *data, uint32_t size) {
    if (addr >= SPU_RAM_SIZE) {
        return 0;
    }
    if (size > SPU_RAM_SIZE - addr) {
        size = SPU_RAM_SIZE - addr;
    }
    lock();
    memcpy(&spu_ram[addr], data, size);
    unlock();
    return size;
}

void spu_ram_read(uint32_t addr, void *out, uint32_t size) {
    if (addr >= SPU_RAM_SIZE) {
        memset(out, 0, size);
        return;
    }
    if (size > SPU_RAM_SIZE - addr) {
        memset((uint8_t *)out + (SPU_RAM_SIZE - addr), 0, size - (SPU_RAM_SIZE - addr));
        size = SPU_RAM_SIZE - addr;
    }
    memcpy(out, &spu_ram[addr], size);
}

/* ---- ADPCM ---- */

static const int adpcm_f0[5] = {0, 60, 115, 98, 122};
static const int adpcm_f1[5] = {0, 0, -52, -55, -60};

void spu_decode_block(const uint8_t *block, int16_t out[28], int16_t *s1, int16_t *s2) {
    int shift;
    int filter;
    int i;

    shift = block[0] & 0x0F;
    if (shift > 12) {
        shift = 9;
    }
    filter = (block[0] >> 4) & 0x07;
    if (filter > 4) {
        filter = 4;
    }
    for (i = 0; i < 28; i++) {
        int nibble = (block[2 + i / 2] >> ((i & 1) * 4)) & 0x0F;
        int32_t sample = (int32_t)(int16_t)(uint16_t)(nibble << 12) >> shift;

        sample += (*s1 * adpcm_f0[filter] + *s2 * adpcm_f1[filter] + 32) / 64;
        if (sample > 32767) {
            sample = 32767;
        } else if (sample < -32768) {
            sample = -32768;
        }
        out[i] = (int16_t)sample;
        *s2 = *s1;
        *s1 = (int16_t)sample;
    }
}

static void decode_next_block(SpuVoice *v) {
    v->addr &= SPU_RAM_SIZE - 1;
    v->block_flags = spu_ram[(v->addr + 1) & (SPU_RAM_SIZE - 1)];
    if (v->block_flags & 0x04) {
        v->repeat = v->addr;
    }
    spu_decode_block(&spu_ram[v->addr], v->block, &v->s1, &v->s2);
    v->block_pos = 0;
}

/* The next source sample; handles block ends and loop flags. */
static int16_t next_sample(SpuVoice *v) {
    int16_t s;

    if (v->block_pos >= 28) {
        if (v->block_flags & 0x01) {
            if (v->block_flags & 0x02) {
                v->addr = v->repeat;
            } else {
                v->env_phase = ENV_OFF;
                v->env_level = 0;
                if (!v->keyed_on) {
                    v->active = 0;
                }
                return 0;
            }
        } else {
            v->addr += 16;
        }
        decode_next_block(v);
    }
    s = v->block[v->block_pos++];
    v->consumed++;
    return s;
}

/* ---- ADSR (PS1 SPU rate encoding) ---- */

static void envelope_tick(SpuVoice *v, int exponential, int decreasing, int shift, int step) {
    int32_t adsr_step;
    int32_t cycles;

    adsr_step = decreasing ? (-8 + step) : (7 - step);
    adsr_step = shift < 11 ? adsr_step << (11 - shift) : adsr_step;
    cycles = shift > 11 ? 1 << (shift - 11) : 1;
    if (exponential && !decreasing && v->env_level > 0x6000) {
        cycles *= 4;
    }
    if (exponential && decreasing) {
        adsr_step = adsr_step * v->env_level / 32768;
    }
    if (++v->env_counter < cycles) {
        return;
    }
    v->env_counter = 0;
    v->env_level += adsr_step;
    if (v->env_level > 0x7FFF) {
        v->env_level = 0x7FFF;
    } else if (v->env_level < 0) {
        v->env_level = 0;
    }
}

static void envelope_step(SpuVoice *v) {
    int sustain_level;

    switch (v->env_phase) {
    case ENV_ATTACK:
        envelope_tick(v, v->adsr1 >> 15, 0, (v->adsr1 >> 10) & 0x1F, (v->adsr1 >> 8) & 3);
        if (v->env_level >= 0x7FFF) {
            v->env_phase = ENV_DECAY;
            v->env_counter = 0;
        }
        break;
    case ENV_DECAY:
        sustain_level = ((v->adsr1 & 0x0F) + 1) * 0x800;
        if (v->env_level <= sustain_level) {
            v->env_phase = ENV_SUSTAIN;
            v->env_counter = 0;
            break;
        }
        envelope_tick(v, 1, 1, (v->adsr1 >> 4) & 0x0F, 0);
        break;
    case ENV_SUSTAIN:
        envelope_tick(v, v->adsr2 >> 15, (v->adsr2 >> 14) & 1, (v->adsr2 >> 8) & 0x1F, (v->adsr2 >> 6) & 3);
        break;
    case ENV_RELEASE:
        envelope_tick(v, (v->adsr2 >> 5) & 1, 1, v->adsr2 & 0x1F, 0);
        if (v->env_level == 0) {
            v->env_phase = ENV_OFF;
            v->active = 0;
        }
        break;
    default:
        break;
    }
}

/* ---- voices ---- */

void spu_voice_start(int voice, uint32_t addr, uint32_t pitch, uint16_t adsr1, uint16_t adsr2, int vol_l, int vol_r) {
    SpuVoice *v;

    if (voice < 0 || voice >= SPU_VOICES) {
        return;
    }
    lock();
    v = &voices[voice];
    memset(v, 0, sizeof(*v));
    v->active = 1;
    v->keyed_on = 1;
    v->addr = addr & ~15u;
    v->repeat = v->addr;
    v->pitch = pitch > 0x3FFF ? 0x3FFF : pitch;
    v->adsr1 = adsr1;
    v->adsr2 = adsr2;
    v->env_phase = ENV_ATTACK;
    v->vol_l = vol_l;
    v->vol_r = vol_r;
    v->order = ++start_counter;
    decode_next_block(v);
    v->cur = next_sample(v);
    unlock();
}

void spu_voice_release(int voice) {
    if (voice < 0 || voice >= SPU_VOICES) {
        return;
    }
    lock();
    if (voices[voice].active) {
        voices[voice].keyed_on = 0;
        if (voices[voice].env_level == 0) {
            voices[voice].active = 0;
            voices[voice].env_phase = ENV_OFF;
        } else {
            voices[voice].env_phase = ENV_RELEASE;
            voices[voice].env_counter = 0;
        }
    }
    unlock();
}

void spu_voice_set_volume(int voice, int vol_l, int vol_r) {
    if (voice < 0 || voice >= SPU_VOICES) {
        return;
    }
    lock();
    voices[voice].vol_l = vol_l;
    voices[voice].vol_r = vol_r;
    unlock();
}

int spu_voice_status(int voice) {
    const SpuVoice *v;

    if (voice < 0 || voice >= SPU_VOICES) {
        return SPU_STATUS_OFF;
    }
    v = &voices[voice];
    if (!v->active) {
        return SPU_STATUS_OFF;
    }
    if (v->keyed_on) {
        return v->env_level > 0 || v->env_phase == ENV_ATTACK ? SPU_STATUS_ON : SPU_STATUS_ENDED;
    }
    return SPU_STATUS_RELEASED;
}

uint32_t spu_voice_start_order(int voice) {
    return voice >= 0 && voice < SPU_VOICES ? voices[voice].order : 0;
}

void spu_set_master(int vol_l, int vol_r) {
    lock();
    master_l = vol_l;
    master_r = vol_r;
    unlock();
}

void spu_reset_voices(void) {
    lock();
    memset(voices, 0, sizeof(voices));
    unlock();
}

int spu_debug_tracing(void) {
    return dump_enabled;
}

uint32_t spu_debug_dump_frames(void) {
    return dump_frames;
}

uint32_t spu_debug_samples_consumed(int voice) {
    return voices[voice].consumed;
}

int spu_debug_envelope(int voice) {
    return voices[voice].env_level;
}

uint32_t spu_debug_pitch(int voice) {
    return voices[voice].pitch;
}

/* ---- mixer ---- */

static void mix_chunk(int16_t *out, int frames) {
    int f;
    int i;

    memset(mix_acc, 0, (size_t)frames * 2 * sizeof(mix_acc[0]));
    for (i = 0; i < SPU_VOICES; i++) {
        SpuVoice *v = &voices[i];

        if (!v->active) {
            continue;
        }
        for (f = 0; f < frames && v->active; f++) {
            int32_t s;

            v->phase += v->pitch;
            while (v->phase >= SPU_PITCH_ONE) {
                v->phase -= SPU_PITCH_ONE;
                v->prev = v->cur;
                v->cur = next_sample(v);
            }
            s = v->prev + (((int32_t)v->cur - v->prev) * (int32_t)v->phase >> 12);
            envelope_step(v);
            s = s * v->env_level >> 15;
            mix_acc[f * 2] += s * v->vol_l >> 14;
            mix_acc[f * 2 + 1] += s * v->vol_r >> 14;
        }
    }
    for (f = 0; f < frames * 2; f++) {
        int32_t s = mix_acc[f] * ((f & 1) ? master_r : master_l) >> 14;

        out[f] = (int16_t)(s > 32767 ? 32767 : (s < -32768 ? -32768 : s));
    }
}

void spu_mix(int16_t *out, int frames) {
    int n;

    while (frames > 0) {
        n = frames > 4096 ? 4096 : frames;
        mix_chunk(out, n);
        out += n * 2;
        frames -= n;
    }
}

void spu_render(int16_t *out, int frames) {
    int n;

    while (frames > 0) {
        n = seq_frames_until_next_event();
        if (n <= 0) {
            seq_run_due_events();
            n = seq_frames_until_next_event();
            if (n <= 0) {
                n = 1;              /* a run of zero-delta events hit the cap: move on */
            }
        }
        if (n > frames) {
            n = frames;
        }
        if (n > 4096) {
            n = 4096;
        }
        spu_mix(out, n);
        seq_advance(n);
        out += n * 2;
        frames -= n;
    }
}

static void SDLCALL audio_callback(void *userdata, Uint8 *stream, int len) {
    int frames;
    uint32_t room;
    uint32_t n;

    (void)userdata;
    frames = len / 4;
    audio_ringbuffer_read((int16_t *)stream, frames);
    if (dump_enabled && dump_frames < DUMP_SECONDS * SPU_RATE) {
        room = DUMP_SECONDS * SPU_RATE - dump_frames;
        n = (uint32_t)frames < room ? (uint32_t)frames : room;
        memcpy(&dump_buf[dump_frames * 2], stream, n * 4);
        dump_frames += n;
    }
}

/* ---- device ---- */

void spu_audio_open(void) {
    SDL_AudioSpec want;
    SDL_AudioSpec have;
    const char *dump;

    dump = getenv("HMBTN_AUDIO_DUMP");
    if (dump != NULL && dump[0] != '\0') {
        snprintf(dump_path, sizeof(dump_path), "%s", dump);
        dump_enabled = 1;
    }
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
        SDL_Log("audio: no audio subsystem (%s); running silent", SDL_GetError());
        return;
    }
    SDL_zero(want);
    want.freq = SPU_RATE;
    want.format = AUDIO_S16SYS;
    want.channels = 2;
    want.samples = 512;
    want.callback = audio_callback;
    audio_dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (audio_dev == 0) {
        SDL_Log("audio: no device (%s); running silent", SDL_GetError());
        return;
    }
    SDL_Log("audio: %d Hz, format 0x%04X, %d channels, %d frames", have.freq, have.format, have.channels,
            have.samples);
    audio_ringbuffer_init();
    audio_worker_start();
    SDL_PauseAudioDevice(audio_dev, 0);
}

int spu_audio_is_open(void) {
    return audio_dev != 0;
}

void spu_audio_pause(int paused) {
    audio_worker_pause(paused);
    if (audio_dev != 0) {
        SDL_PauseAudioDevice(audio_dev, paused);
    }
}

static void write_le16(FILE *f, uint16_t v) {
    fputc(v & 0xFF, f);
    fputc(v >> 8, f);
}

static void write_le32(FILE *f, uint32_t v) {
    write_le16(f, (uint16_t)(v & 0xFFFF));
    write_le16(f, (uint16_t)(v >> 16));
}

static void write_dump(void) {
    FILE *f;
    uint32_t bytes;

    f = fopen(dump_path, "wb");
    if (f == NULL) {
        SDL_Log("audio: cannot write %s", dump_path);
        return;
    }
    bytes = dump_frames * 4;
    fwrite("RIFF", 1, 4, f);
    write_le32(f, 36 + bytes);
    fwrite("WAVEfmt ", 1, 8, f);
    write_le32(f, 16);
    write_le16(f, 1);
    write_le16(f, 2);
    write_le32(f, SPU_RATE);
    write_le32(f, SPU_RATE * 4);
    write_le16(f, 4);
    write_le16(f, 16);
    fwrite("data", 1, 4, f);
    write_le32(f, bytes);
    fwrite(dump_buf, 1, bytes, f);
    fclose(f);
    SDL_Log("audio: wrote %u frames to %s", (unsigned)dump_frames, dump_path);
}

void spu_audio_close(void) {
    audio_worker_stop();
    audio_ringbuffer_clear();
    if (audio_dev != 0) {
        SDL_CloseAudioDevice(audio_dev);
        audio_dev = 0;
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        SDL_Log("audio closed");
    }
    if (dump_enabled) {
        write_dump();
        dump_enabled = 0;
    }
}
