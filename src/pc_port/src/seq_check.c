/* SEQ player checks, run at startup when HMBTN_SELFTEST=1. They step synthetic sequences
 * on a synthetic VAB by sample counts with the audio device paused. */

#include <libsnd.h>
#include <seq_player.h>
#include <spu_core.h>

#include <SDL.h>

#include <stdlib.h>
#include <string.h>

#define ADSR1_FAST 0x000F
#define ADSR2_HOLD_FAST 0x1F00

static u_char vab_header[32 + 128 * 16 + 2 * 16 * 32 + 512];
static u_char vab_body[32];
static uint8_t seq_buf[SEQ_SLOT_SIZE + 64];
static int16_t render_buf[4096 * 2];

static int check(int ok, const char *what) {
    SDL_Log("seq check %s: %s", ok ? "ok" : "FAILED", what);
    return ok ? 0 : 1;
}

/* Program 0: center 60; program 1: center 48. One looping VAG, keys 0..127. */
static short open_vab(void) {
    VabHdr hdr;
    ProgAtr *prog;
    VagAtr *tone;
    uint16_t *sizes;
    short id;
    int i;

    memset(vab_header, 0, sizeof(vab_header));
    memset(&hdr, 0, sizeof(hdr));
    hdr.form = 0x56414270;
    hdr.ps = 2;
    hdr.ts = 2;
    hdr.vs = 1;
    hdr.mvol = 127;
    hdr.pan = 64;
    memcpy(vab_header, &hdr, sizeof(hdr));
    prog = (ProgAtr *)(vab_header + 32);
    tone = (VagAtr *)(vab_header + 32 + 128 * 16);
    for (i = 0; i < 2; i++) {
        prog[i].tones = 1;
        prog[i].mvol = 127;
        prog[i].mpan = 64;
        tone[i * 16].vol = 127;
        tone[i * 16].pan = 64;
        tone[i * 16].center = (u_char)(i == 0 ? 60 : 48);
        tone[i * 16].min = 0;
        tone[i * 16].max = 127;
        tone[i * 16].adsr1 = ADSR1_FAST;
        tone[i * 16].adsr2 = ADSR2_HOLD_FAST;
        tone[i * 16].vag = 1;
    }
    sizes = (uint16_t *)(vab_header + 32 + 128 * 16 + 2 * 16 * 32);
    sizes[1] = 4;
    memset(vab_body, 0x33, sizeof(vab_body));
    vab_body[0] = 0x00;
    vab_body[1] = 0x04;
    vab_body[16] = 0x00;
    vab_body[17] = 0x03;
    id = SsVabOpenHead(vab_header, -1);
    SsVabTransBodyPartly(vab_body, sizeof(vab_body), id);
    return id;
}

/* A SEQ with the given event bytes after the 15-byte header. */
static u_long *make_seq(const uint8_t *events, int n, int resolution, uint32_t tempo) {
    memset(seq_buf, 0, 64 + (size_t)n);
    memcpy(seq_buf, "pQES", 4);
    seq_buf[7] = 1;
    seq_buf[8] = (uint8_t)(resolution >> 8);
    seq_buf[9] = (uint8_t)resolution;
    seq_buf[10] = (uint8_t)(tempo >> 16);
    seq_buf[11] = (uint8_t)(tempo >> 8);
    seq_buf[12] = (uint8_t)tempo;
    seq_buf[13] = 4;
    seq_buf[14] = 2;
    memcpy(seq_buf + 15, events, (size_t)n);
    return (u_long *)seq_buf;
}

static int voices_on(void) {
    int n;
    int i;

    n = 0;
    for (i = 0; i < SPU_VOICES; i++) {
        n += spu_voice_status(i) == SPU_STATUS_ON;
    }
    return n;
}

static int newest_voice(void) {
    int best;
    int i;

    best = 0;
    for (i = 1; i < SPU_VOICES; i++) {
        if (spu_voice_start_order(i) > spu_voice_start_order(best)) {
            best = i;
        }
    }
    return best;
}

static void render(int frames) {
    int n;

    while (frames > 0) {
        n = frames > 4096 ? 4096 : frames;
        spu_render(render_buf, n);
        frames -= n;
    }
}

static short play(short vab, const uint8_t *events, int n, uint32_t tempo, short count) {
    short h;

    spu_reset_voices();
    h = SsSeqOpen(make_seq(events, n, 480, tempo), vab);
    SsSeqPlay(h, 1, count);
    return h;
}

static int check_open(short vab) {
    static const uint8_t ok_events[] = {0x00, 0xFF, 0x2F, 0x00};
    short h;
    int failed;

    h = SsSeqOpen(make_seq(ok_events, sizeof(ok_events), 480, 500000), vab);
    failed = check(h == 0 && seq_debug_resolution(h) == 480 && seq_debug_tempo(h) == 500000,
                   "a valid SEQ opens as handle 0 with resolution 480 and tempo 500000");
    SsSeqClose(h);
    memset(seq_buf, 0, sizeof(seq_buf));
    failed |= check(SsSeqOpen((u_long *)seq_buf, vab) == -1, "a stream without pQES returns -1");
    memcpy(seq_buf, "pQES", 4);
    failed |= check(SsSeqOpen((u_long *)seq_buf, vab) == -1, "a stream without End of Track returns -1");
    h = SsSeqOpen(make_seq(ok_events, sizeof(ok_events), 480, 500000), vab);
    failed |= check(h == 0, "closing frees the slot for the next open");
    SsSeqClose(h);
    return failed;
}

static int check_events(short vab) {
    static const uint8_t running[] = {0x00, 0x90, 0x3C, 0x64, 0x00, 0x3E, 0x64, 0x00, 0x40, 0x64,
                                      0x83, 0x60, 0xFF, 0x2F, 0x00};
    static const uint8_t bend[] = {0x00, 0x90, 0x3C, 0x64, 0x00, 0xE0, 0x00, 0x40, 0x00, 0x90, 0x3E, 0x64,
                                   0x83, 0x60, 0xFF, 0x2F, 0x00};
    static const uint8_t program[] = {0x00, 0xC1, 0x01, 0x00, 0x91, 0x3C, 0x64, 0x83, 0x60, 0xFF, 0x2F, 0x00};
    short h;
    int failed;

    h = play(vab, running, sizeof(running), 500000, 0);
    render(1);
    failed = check(voices_on() == 3, "three running-status notes start three voices");
    SsSeqClose(h);

    h = play(vab, bend, sizeof(bend), 500000, 0);
    render(1);
    failed |= check(voices_on() == 2, "a pitch bend between two notes is skipped and both notes start");
    SsSeqClose(h);

    h = play(vab, program, sizeof(program), 500000, 0);
    render(1);
    failed |= check(spu_debug_pitch(newest_voice()) == 8192, "a Program Change switches the tone of the next note");
    SsSeqClose(h);
    return failed;
}

static int check_timing(short vab) {
    static const uint8_t quarter[] = {0x83, 0x60, 0x90, 0x3C, 0x64, 0x83, 0x60, 0xFF, 0x2F, 0x00};
    static const uint8_t faster[] = {0x00, 0xFF, 0x51, 0x03, 0x03, 0xD0, 0x90, 0x83, 0x60, 0x90, 0x3C, 0x64,
                                     0x83, 0x60, 0xFF, 0x2F, 0x00};
    short h;
    int failed;

    h = play(vab, quarter, sizeof(quarter), 500000, 1);
    render(22050);
    failed = check(voices_on() == 0, "a Note On at tick 480 has not started within the first 22050 frames");
    render(1);
    failed |= check(voices_on() == 1, "it starts on frame 22050");
    SsSeqClose(h);

    h = play(vab, faster, sizeof(faster), 500000, 1);
    render(11025);
    failed |= check(voices_on() == 0, "after a Tempo event to 250000, 480 ticks have not passed in 11025 frames");
    render(1);
    failed |= check(voices_on() == 1, "and the note starts on frame 11025");
    SsSeqClose(h);
    return failed;
}

static int check_notes(short vab) {
    static const uint8_t on_off[] = {0x00, 0x90, 0x3C, 0x64, 0x83, 0x60, 0x80, 0x3C, 0x00, 0x83, 0x60, 0xFF, 0x2F, 0x00};
    static const uint8_t chord[] = {0x00, 0x90, 0x3C, 0x64, 0x00, 0x90, 0x3E, 0x64, 0x00, 0x90, 0x48, 0x64,
                                    0x83, 0x60, 0xFF, 0x2F, 0x00};
    static const uint8_t silent[] = {0x00, 0xB0, 0x07, 0x00, 0x00, 0x90, 0x3C, 0x64, 0x83, 0x60, 0xFF, 0x2F, 0x00};
    short h;
    int failed;
    int peak;
    int i;

    h = play(vab, on_off, sizeof(on_off), 500000, 1);
    render(10);
    failed = check(spu_voice_status(0) == SPU_STATUS_ON, "a note's voice reports 1 before its Note Off");
    render(22040);
    render(1);
    failed |= check(spu_voice_status(0) == SPU_STATUS_RELEASED, "and 2 after it");
    SsSeqClose(h);

    SsSetReservedVoice(2);
    h = play(vab, chord, sizeof(chord), 500000, 1);
    render(1);
    failed |= check(spu_debug_pitch(0) == 8192, "with every reserved voice busy, the next note takes the earliest one");
    SsSeqClose(h);
    SsSetReservedVoice(24);

    h = play(vab, silent, sizeof(silent), 500000, 1);
    render(1);
    spu_render(render_buf, 200);
    peak = 0;
    for (i = 0; i < 400; i++) {
        peak = abs(render_buf[i]) > peak ? abs(render_buf[i]) : peak;
    }
    failed |= check(voices_on() == 1 && peak == 0, "Control Change 7 at 0 makes the next note silent");
    SsSeqClose(h);
    return failed;
}

static int check_loops(short vab) {
    static const uint8_t marked[] = {0x00, 0xB0, 0x63, 0x14, 0x00, 0xB0, 0x06, 0x00, 0x00, 0x90, 0x3C, 0x64,
                                     0x81, 0x70, 0x80, 0x3C, 0x00, 0x00, 0xB0, 0x63, 0x1E, 0x00, 0xFF, 0x2F, 0x00};
    static const uint8_t plain[] = {0x00, 0x90, 0x3C, 0x64, 0x81, 0x70, 0x80, 0x3C, 0x00, 0x00, 0xFF, 0x2F, 0x00};
    short h;
    int failed;

    h = play(vab, marked, sizeof(marked), 500000, 0);
    render(40000);
    failed = check(seq_debug_note_ons(h) >= 3, "loop markers bring the loop's first note back after the loop end");
    SsSeqClose(h);

    h = play(vab, plain, sizeof(plain), 500000, 0);
    render(40000);
    failed |= check(seq_debug_note_ons(h) >= 3, "with count 0, End of Track restarts the sequence");
    SsSeqClose(h);
    return failed;
}

static int check_controls(short vab) {
    static const uint8_t chord[] = {0x00, 0x90, 0x3C, 0x64, 0x00, 0x90, 0x3E, 0x64, 0x00, 0x90, 0x40, 0x64,
                                    0x83, 0x60, 0xFF, 0x2F, 0x00};
    static const uint8_t steps[] = {0x00, 0x90, 0x3C, 0x64, 0x83, 0x60, 0x90, 0x3E, 0x64, 0x83, 0x60, 0x90, 0x40, 0x64,
                                    0x83, 0x60, 0xFF, 0x2F, 0x00};
    short h;
    int failed;
    int i;
    int released;

    h = play(vab, chord, sizeof(chord), 500000, 0);
    render(10);
    SsSeqStop(h);
    released = 1;
    for (i = 0; i < 3; i++) {
        released &= spu_voice_status(i) == SPU_STATUS_RELEASED || spu_voice_status(i) == SPU_STATUS_OFF;
    }
    failed = check(released, "stop releases every voice the sequence held");
    SsSeqClose(h);

    h = play(vab, steps, sizeof(steps), 500000, 0);
    render(10);
    render(22030);
    SsSeqPause(h);
    render(30000);
    failed |= check(seq_debug_note_ons(h) == 1, "a paused sequence does not advance");
    SsSeqReplay(h);
    render(30);
    failed |= check(seq_debug_note_ons(h) == 2 && spu_debug_pitch(newest_voice()) == 4598,
                    "replay continues with the next event, not the first");
    SsSeqClose(h);
    return failed;
}

int seq_self_check(void) {
    short vab;
    int failed;

    spu_audio_pause(1);
    spu_reset_voices();
    vab = open_vab();
    failed = check(vab >= 0, "the synthetic VAB opens");
    if (vab >= 0) {
        failed |= check_open(vab);
        failed |= check_events(vab);
        failed |= check_timing(vab);
        failed |= check_notes(vab);
        failed |= check_loops(vab);
        failed |= check_controls(vab);
        SsVabClose(vab);
    }
    spu_reset_voices();
    spu_audio_pause(0);
    return failed;
}
