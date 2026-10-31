/* The host SEQ player. See seq_player.h. */

#include <seq_player.h>
#include <libsnd.h>
#include <spu_core.h>

#include <SDL.h>

#include <math.h>
#include <stddef.h>
#include <string.h>

#define SEQ_MAGIC 0x53455170u   /* "pQES" read little-endian */
#define SEQ_HEADER 15
#define SEQ_EVENT_CAP 4096
#define NO_NOTE -1
/* The mapped PS1 RAM window: the End of Track scan stops at its end. */
#define PS1_RAM_START 0x80000000u
#define PS1_RAM_END 0x80200000u

typedef struct {
    int used;
    short vab;
    uint32_t len;
    uint16_t resolution;
    uint32_t tempo0;

    int playing;
    int paused;
    int plays_left;     /* 0 = forever */
    uint32_t pos;
    uint8_t running;
    uint32_t tempo;
    double samples_to_next;

    uint32_t loop_pos;
    int loop_set;
    int loop_count;
    int loop_left;
    int loop_count_pending;

    int vol_l;
    int vol_r;
    uint8_t ch_prog[16];
    uint8_t ch_vol[16];
    uint8_t ch_pan[16];

    int held[SPU_VOICES];      /* (channel << 8) | note, or NO_NOTE */
    int base_l[SPU_VOICES];
    int base_r[SPU_VOICES];
    uint32_t note_ons;

    uint8_t data[SEQ_SLOT_SIZE];
} Seq;

static Seq seqs[SEQ_SLOTS];

static uint16_t be16(const uint8_t *p) {
    return (uint16_t)((p[0] << 8) | p[1]);
}

static uint32_t be24(const uint8_t *p) {
    return ((uint32_t)p[0] << 16) | ((uint32_t)p[1] << 8) | p[2];
}

static int valid_handle(short h) {
    return h >= 0 && h < SEQ_SLOTS && seqs[h].used;
}

/* Length of the stream up to and including End of Track, or 0 if there is none
 * within `max` bytes. */
static uint32_t stream_length(const uint8_t *s, uint32_t max) {
    uint32_t i;

    for (i = 0; i + 2 < max; i++) {
        if (s[i] == 0xFF && s[i + 1] == 0x2F && s[i + 2] == 0x00) {
            return i + 3;
        }
    }
    return 0;
}

/* ---- voices ---- */

static void release_held(Seq *s, int match) {
    int v;

    for (v = 0; v < SPU_VOICES; v++) {
        if (s->held[v] != NO_NOTE && (match == NO_NOTE || s->held[v] == match)) {
            spu_voice_release(v);
            s->held[v] = NO_NOTE;
        }
    }
}

void seq_voice_taken(int voice) {
    int i;

    if (voice < 0 || voice >= SPU_VOICES) {
        return;
    }
    for (i = 0; i < SEQ_SLOTS; i++) {
        seqs[i].held[voice] = NO_NOTE;
    }
}

static void note_on(Seq *s, int ch, int note, int velocity) {
    int tones;
    int t;
    int vol;
    int min;
    int max;

    tones = snd_program_tones(s->vab, s->ch_prog[ch]);
    vol = velocity * s->ch_vol[ch] / 127;
    for (t = 0; t < tones; t++) {
        int voice;
        int i;

        if (snd_tone_range(s->vab, s->ch_prog[ch], (short)t, &min, &max) != 0 || note < min || note > max) {
            continue;
        }
        voice = snd_pick_voice();
        if (voice < 0) {
            return;
        }
        for (i = 0; i < SEQ_SLOTS; i++) {
            seqs[i].held[voice] = NO_NOTE;
        }
        if (snd_key_on_ex((short)voice, s->vab, s->ch_prog[ch], (short)t, (short)note, 0, (short)vol, (short)vol,
                          s->ch_pan[ch] - 64, &s->base_l[voice], &s->base_r[voice]) < 0) {
            continue;
        }
        if (s->vol_l != 127 || s->vol_r != 127) {
            spu_voice_set_volume(voice, s->base_l[voice] * s->vol_l / 127, s->base_r[voice] * s->vol_r / 127);
        }
        s->held[voice] = (ch << 8) | note;
        s->note_ons++;
    }
}

/* ---- events ---- */

static double samples_per_tick(const Seq *s) {
    return (double)s->tempo * SPU_RATE / ((double)s->resolution * 1000000.0);
}

static int read_byte(Seq *s, uint8_t *b) {
    if (s->pos >= s->len) {
        return 0;
    }
    *b = s->data[s->pos++];
    return 1;
}

/* Reads the delta before the next event and schedules it. */
static void schedule_next(Seq *s) {
    uint32_t delta;
    uint8_t b;
    int n;

    delta = 0;
    for (n = 0; n < 4; n++) {
        if (!read_byte(s, &b)) {
            break;
        }
        delta = (delta << 7) | (b & 0x7F);
        if (!(b & 0x80)) {
            break;
        }
    }
    s->samples_to_next += delta * samples_per_tick(s);
}

static void rewind_seq(Seq *s) {
    int ch;

    s->pos = 0;
    s->running = 0;
    s->tempo = s->tempo0;
    s->loop_set = 0;
    s->loop_count_pending = 0;
    for (ch = 0; ch < 16; ch++) {
        s->ch_prog[ch] = 0;
        s->ch_vol[ch] = 127;
        s->ch_pan[ch] = 64;
    }
}

static void end_of_track(Seq *s) {
    if (s->plays_left == 0 || --s->plays_left > 0) {
        s->pos = 0;
        s->running = 0;
        s->loop_set = 0;
        return;
    }
    s->playing = 0;
    release_held(s, NO_NOTE);
}

static void control_change(Seq *s, int ch, int controller, int value) {
    switch (controller) {
    case 0x07:
        s->ch_vol[ch] = (uint8_t)value;
        break;
    case 0x0A:
        s->ch_pan[ch] = (uint8_t)value;
        break;
    case 0x06:
        if (s->loop_count_pending) {
            s->loop_count = value;
            s->loop_left = value;
            s->loop_count_pending = 0;
        }
        break;
    case 0x63:
        if (value == 20) {
            s->loop_pos = s->pos;
            s->loop_set = 1;
            s->loop_count = 0;
            s->loop_left = 0;
            s->loop_count_pending = 1;
        } else if (value == 30 && s->loop_set) {
            if (s->loop_count == 0 || --s->loop_left > 0) {
                s->pos = s->loop_pos;
                s->running = 0;
            }
        }
        break;
    default:
        break;
    }
}

/* Runs the event at `pos` (after its delta), then schedules the next one. */
static void run_event(Seq *s) {
    uint8_t status;
    uint8_t d1;
    uint8_t d2;
    uint8_t type;
    uint8_t len;
    int ch;
    uint32_t i;

    if (!read_byte(s, &status)) {
        end_of_track(s);
        if (s->playing) {
            schedule_next(s);
        }
        return;
    }
    if (status < 0x80) {
        d1 = status;
        status = s->running;
    } else if (status < 0xF0) {
        s->running = status;
        if (!read_byte(s, &d1)) {
            d1 = 0;
        }
    } else {
        d1 = 0;
    }
    ch = status & 0x0F;
    switch (status & 0xF0) {
    case 0x90:
        read_byte(s, &d2);
        if (d2 > 0) {
            note_on(s, ch, d1, d2);
        } else {
            release_held(s, (ch << 8) | d1);
        }
        break;
    case 0x80:
        read_byte(s, &d2);
        release_held(s, (ch << 8) | d1);
        break;
    case 0xA0:
    case 0xE0:
        read_byte(s, &d2);
        break;
    case 0xB0:
        read_byte(s, &d2);
        control_change(s, ch, d1, d2);
        break;
    case 0xC0:
        s->ch_prog[ch] = d1;
        break;
    case 0xD0:
        break;
    case 0xF0:
        if (status == 0xFF) {
            read_byte(s, &type);
            read_byte(s, &len);
            if (type == 0x2F) {
                end_of_track(s);
                if (s->playing) {
                    schedule_next(s);
                }
                return;
            }
            if (type == 0x51 && len == 3 && s->pos + 3 <= s->len) {
                s->tempo = be24(&s->data[s->pos]);
            }
            for (i = 0; i < len; i++) {
                read_byte(s, &d2);
            }
        } else if (status == 0xF0 || status == 0xF7) {
            read_byte(s, &len);
            for (i = 0; i < len; i++) {
                read_byte(s, &d2);
            }
        }
        break;
    default:
        break;
    }
    schedule_next(s);
}

/* ---- audio-thread interface ---- */

int seq_frames_until_next_event(void) {
    int best;
    int i;

    best = SEQ_NO_EVENT;
    for (i = 0; i < SEQ_SLOTS; i++) {
        const Seq *s = &seqs[i];

        if (s->used && s->playing && !s->paused) {
            double n = ceil(s->samples_to_next);

            if (n < best) {
                best = n <= 0 ? 0 : (int)n;
            }
        }
    }
    return best;
}

void seq_run_due_events(void) {
    int i;
    int events;

    for (i = 0; i < SEQ_SLOTS; i++) {
        Seq *s = &seqs[i];

        events = 0;
        while (s->used && s->playing && !s->paused && s->samples_to_next <= 0.0 && events < SEQ_EVENT_CAP) {
            run_event(s);
            events++;
        }
    }
}

void seq_advance(int frames) {
    int i;

    for (i = 0; i < SEQ_SLOTS; i++) {
        Seq *s = &seqs[i];

        if (s->used && s->playing && !s->paused) {
            s->samples_to_next -= frames;
        }
    }
}

/* ---- the game's calls ---- */

short SsSeqOpen(u_long *addr, short vab_id) {
    const uint8_t *p;
    uint32_t len;
    int h;
    uint32_t magic;

    p = (const uint8_t *)addr;
    memcpy(&magic, p, 4);
    if (magic != SEQ_MAGIC) {
        return -1;
    }
    len = SEQ_SLOT_SIZE;
    if ((uintptr_t)p >= PS1_RAM_START && (uintptr_t)p < PS1_RAM_END && PS1_RAM_END - (uintptr_t)p - SEQ_HEADER < len) {
        len = (uint32_t)(PS1_RAM_END - (uintptr_t)p - SEQ_HEADER);
    }
    len = stream_length(p + SEQ_HEADER, len);
    if (len == 0) {
        SDL_Log("audio: SEQ refused (no End of Track within %d bytes)", SEQ_SLOT_SIZE);
        return -1;
    }
    for (h = 0; h < SEQ_SLOTS; h++) {
        if (!seqs[h].used) {
            break;
        }
    }
    if (h == SEQ_SLOTS) {
        return -1;
    }
    spu_lock();
    memset(&seqs[h], 0, offsetof(Seq, data));
    memcpy(seqs[h].data, p + SEQ_HEADER, len);
    seqs[h].len = len;
    seqs[h].vab = vab_id;
    seqs[h].resolution = be16(p + 8) != 0 ? be16(p + 8) : 480;
    seqs[h].tempo0 = be24(p + 10) != 0 ? be24(p + 10) : 500000;
    seqs[h].vol_l = 127;
    seqs[h].vol_r = 127;
    memset(seqs[h].held, 0xFF, sizeof(seqs[h].held));
    rewind_seq(&seqs[h]);
    seqs[h].used = 1;
    spu_unlock();
    if (spu_debug_tracing()) {
        SDL_Log("audio: SEQ %d opened, vab %d, resolution %u, tempo %u us, %u bytes", h, vab_id,
                (unsigned)seqs[h].resolution, (unsigned)seqs[h].tempo0, (unsigned)len);
    }
    return (short)h;
}

void SsSeqClose(short seq_access_num) {
    if (!valid_handle(seq_access_num)) {
        return;
    }
    spu_lock();
    release_held(&seqs[seq_access_num], NO_NOTE);
    seqs[seq_access_num].used = 0;
    spu_unlock();
}

void SsSeqPlay(short seq_access_num, char play_mode, short l_count) {
    Seq *s;

    if (!valid_handle(seq_access_num)) {
        return;
    }
    (void)play_mode;
    spu_lock();
    s = &seqs[seq_access_num];
    release_held(s, NO_NOTE);
    rewind_seq(s);
    s->plays_left = l_count < 0 ? 0 : l_count;
    s->samples_to_next = 0.0;
    s->paused = 0;
    s->playing = 1;
    schedule_next(s);
    spu_unlock();
    if (spu_debug_tracing()) {
        SDL_Log("audio: SEQ %d play, count %d", seq_access_num, l_count);
    }
}

void SsSeqStop(short seq_access_num) {
    Seq *s;

    if (!valid_handle(seq_access_num)) {
        return;
    }
    spu_lock();
    s = &seqs[seq_access_num];
    release_held(s, NO_NOTE);
    s->playing = 0;
    s->paused = 0;
    rewind_seq(s);
    spu_unlock();
}

void SsSeqPause(short seq_access_num) {
    if (!valid_handle(seq_access_num)) {
        return;
    }
    spu_lock();
    if (seqs[seq_access_num].playing) {
        seqs[seq_access_num].paused = 1;
        release_held(&seqs[seq_access_num], NO_NOTE);
    }
    spu_unlock();
}

void SsSeqReplay(short seq_access_num) {
    if (!valid_handle(seq_access_num)) {
        return;
    }
    spu_lock();
    seqs[seq_access_num].paused = 0;
    spu_unlock();
}

void SsSeqSetVol(short seq_access_num, short voll, short volr) {
    Seq *s;
    int v;

    if (!valid_handle(seq_access_num)) {
        return;
    }
    spu_lock();
    s = &seqs[seq_access_num];
    s->vol_l = voll < 0 ? 0 : (voll > 127 ? 127 : voll);
    s->vol_r = volr < 0 ? 0 : (volr > 127 ? 127 : volr);
    for (v = 0; v < SPU_VOICES; v++) {
        if (s->held[v] != NO_NOTE) {
            spu_voice_set_volume(v, s->base_l[v] * s->vol_l / 127, s->base_r[v] * s->vol_r / 127);
        }
    }
    spu_unlock();
}

int seq_debug_resolution(short handle) {
    return valid_handle(handle) ? seqs[handle].resolution : -1;
}

uint32_t seq_debug_tempo(short handle) {
    return valid_handle(handle) ? seqs[handle].tempo0 : 0;
}

uint32_t seq_debug_note_ons(short handle) {
    return valid_handle(handle) ? seqs[handle].note_ons : 0;
}
