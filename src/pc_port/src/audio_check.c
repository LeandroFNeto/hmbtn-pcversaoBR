/* SPU, libsnd, and libspu checks, run at startup when HMBTN_SELFTEST=1. The device is
 * paused while they mix by hand; voices, master volume, and VABs are reset afterwards. */

#include <libsnd.h>
#include <libspu.h>
#include <spu_core.h>

#include <SDL.h>

#include <stdlib.h>
#include <string.h>

uint32_t snd_debug_vab_base(short vabid);

#define TEST_ADDR 0x20000u
#define ADSR1_FAST 0x000F      /* linear attack shift 0, decay shift 0, sustain level 15 */
#define ADSR2_HOLD_FAST 0x1F00 /* sustain held (shift 31), linear release shift 0 */
#define ADSR2_HOLD_SLOW 0x1F1F /* sustain held, linear release shift 31 */

static int16_t mix_buf[4096 * 2];

static int check(int ok, const char *what) {
    SDL_Log("audio check %s: %s", ok ? "ok" : "FAILED", what);
    return ok ? 0 : 1;
}

/* A 16-byte ADPCM block: filter 0, `shift`, every nibble `nibble`, flags `flags`. */
static void make_block(uint8_t *b, int shift, int nibble, uint8_t flags) {
    int i;

    b[0] = (uint8_t)shift;
    b[1] = flags;
    for (i = 2; i < 16; i++) {
        b[i] = (uint8_t)(nibble | (nibble << 4));
    }
}

static void put_blocks(uint32_t addr, const uint8_t flags[], int count, int shift, int nibble) {
    uint8_t b[16];
    int i;

    for (i = 0; i < count; i++) {
        make_block(b, shift, nibble, flags[i]);
        spu_ram_write(addr + (uint32_t)i * 16, b, 16);
    }
}

static int peak(int frames) {
    int m;
    int i;

    m = 0;
    for (i = 0; i < frames * 2; i++) {
        int a = abs(mix_buf[i]);

        m = a > m ? a : m;
    }
    return m;
}

static int check_core(void) {
    static const uint8_t one_shot[2] = {0x00, 0x01};
    static const uint8_t looping[2] = {0x04, 0x03};
    uint8_t in[32];
    uint8_t out[32];
    uint8_t block[16];
    int16_t pcm[28];
    int16_t s1;
    int16_t s2;
    int failed;
    int i;

    for (i = 0; i < 32; i++) {
        in[i] = (uint8_t)(i * 7 + 1);
    }
    spu_ram_write(0x1010, in, 32);
    spu_ram_read(0x1010, out, 32);
    failed = check(memcmp(in, out, 32) == 0, "32 bytes at 0x1010 read back equal");
    failed |= check(spu_ram_write(SPU_RAM_SIZE - 16, in, 32) == 16, "a write 16 bytes before the end writes 16 bytes");

    memset(block, 0, sizeof(block));
    block[0] = 0x0C;
    block[2] = 0x71;                        /* nibbles 1, 7 */
    block[3] = 0xF8;                        /* nibbles 8, 15 */
    s1 = s2 = 0;
    spu_decode_block(block, pcm, &s1, &s2);
    failed |= check(pcm[0] == 1 && pcm[1] == 7 && pcm[2] == -8 && pcm[3] == -1,
                    "shift 12 / filter 0 nibbles 1, 7, 8, 15 decode to 1, 7, -8, -1");

    /* One-shot: two blocks, the second ends without repeat. */
    put_blocks(TEST_ADDR, one_shot, 2, 0, 3);
    spu_voice_start(0, TEST_ADDR, SPU_PITCH_ONE, ADSR1_FAST, ADSR2_HOLD_FAST, 0x3FFF, 0x3FFF);
    spu_mix(mix_buf, 100);
    failed |= check(spu_voice_status(0) == SPU_STATUS_ENDED, "a one-shot sample ends its voice (status 3)");
    spu_mix(mix_buf, 64);
    failed |= check(peak(64) == 0, "an ended one-shot is silent");
    failed |= check(spu_voice_status(0) == SPU_STATUS_ENDED, "an ended one-shot not keyed off reports 3");
    spu_voice_release(0);
    failed |= check(spu_voice_status(0) == SPU_STATUS_OFF, "keying off an ended voice turns it off");

    /* Looping: flag 0x04 on the first block, 0x03 on the last. */
    put_blocks(TEST_ADDR + 0x100, looping, 2, 0, 3);
    spu_voice_start(1, TEST_ADDR + 0x100, SPU_PITCH_ONE, ADSR1_FAST, ADSR2_HOLD_FAST, 0x3FFF, 0x3FFF);
    spu_mix(mix_buf, 56 * 3 + 10);
    failed |= check(spu_voice_status(1) == SPU_STATUS_ON && peak(10) > 0,
                    "a looping sample still sounds after three times its length");

    /* Envelope: fast attack reaches a level, release reaches zero. */
    failed |= check(spu_debug_envelope(1) > 0, "a fast attack reaches a non-zero envelope within 100 samples");
    spu_voice_release(1);
    failed |= check(spu_voice_status(1) == SPU_STATUS_RELEASED, "a keyed-off voice still sounding reports 2");
    spu_mix(mix_buf, 100);
    failed |= check(spu_voice_status(1) == SPU_STATUS_OFF && spu_debug_envelope(1) == 0,
                    "after release the envelope reaches zero and the voice reports 0");

    /* Pitch: 8192 consumes two source samples per output sample. */
    spu_voice_start(2, TEST_ADDR + 0x100, 8192, ADSR1_FAST, ADSR2_HOLD_FAST, 0x3FFF, 0x3FFF);
    {
        uint32_t before = spu_debug_samples_consumed(2);

        spu_mix(mix_buf, 100);
        failed |= check(spu_debug_samples_consumed(2) - before == 200, "pitch 8192 consumes two source samples per output");
    }
    spu_voice_release(2);
    spu_mix(mix_buf, 50);

    /* Volume 0 adds nothing; two loud voices clamp. */
    put_blocks(TEST_ADDR + 0x200, looping, 2, 0, 7);
    spu_voice_start(3, TEST_ADDR + 0x200, SPU_PITCH_ONE, ADSR1_FAST, ADSR2_HOLD_FAST, 0, 0);
    spu_mix(mix_buf, 64);
    failed |= check(peak(64) == 0, "a voice at volume 0 adds nothing");
    spu_voice_set_volume(3, 0x3FFF, 0x3FFF);
    spu_voice_start(4, TEST_ADDR + 0x200, SPU_PITCH_ONE, ADSR1_FAST, ADSR2_HOLD_FAST, 0x3FFF, 0x3FFF);
    spu_mix(mix_buf, 64);
    failed |= check(peak(32) == 32767 || peak(32) == 32768, "two loud voices sum and clamp to 16 bits");
    spu_reset_voices();
    return failed;
}

/* A VAB with 2 programs (one tone each) and 3 VAGs of 32, 16, and 16 bytes. */
static u_char vab_header[32 + 128 * 16 + 2 * 16 * 32 + 512];
static u_char vab_body[64];

static void build_vab(void) {
    VabHdr hdr;
    ProgAtr *prog;
    VagAtr *tone;
    uint16_t *sizes;
    int i;

    memset(vab_header, 0, sizeof(vab_header));
    memset(&hdr, 0, sizeof(hdr));
    hdr.form = 0x56414270;
    hdr.ps = 2;
    hdr.ts = 2;
    hdr.vs = 3;
    hdr.mvol = 127;
    hdr.pan = 64;
    memcpy(vab_header, &hdr, sizeof(hdr));
    prog = (ProgAtr *)(vab_header + 32);
    for (i = 0; i < 2; i++) {
        prog[i].tones = 1;
        prog[i].mvol = 127;
        prog[i].mpan = 64;
    }
    tone = (VagAtr *)(vab_header + 32 + 128 * 16);
    for (i = 0; i < 2; i++) {
        tone[i * 16].vol = 127;
        tone[i * 16].pan = 64;
        tone[i * 16].center = 60;
        tone[i * 16].adsr1 = ADSR1_FAST;
        tone[i * 16].adsr2 = ADSR2_HOLD_FAST;
        tone[i * 16].vag = (short)(i + 1);
    }
    sizes = (uint16_t *)(vab_header + 32 + 128 * 16 + 2 * 16 * 32);
    sizes[1] = 4;
    sizes[2] = 2;
    sizes[3] = 2;
    for (i = 0; i < 64; i++) {
        vab_body[i] = (uint8_t)(0x40 + i);
    }
    vab_body[1] = 0x04;                      /* VAG 1: two blocks that loop */
    vab_body[17] = 0x03;
}

static int check_libsnd(void) {
    u_char bad[64];
    u_char back[64];
    uint32_t base;
    short id;
    short id2;
    int failed;
    int i;

    build_vab();
    id = SsVabOpenHead(vab_header, -1);
    failed = check(id >= 0 && id < 16, "a VAB header opens with an id from 0 to 15");
    memset(bad, 0, sizeof(bad));
    failed |= check(SsVabOpenHead(bad, -1) == -1, "a header without pBAV returns -1");
    if (id < 0) {
        return failed;
    }
    base = snd_debug_vab_base(id);
    SsVabTransBodyPartly(vab_body, 32, id);
    SsVabTransBodyPartly(vab_body + 32, 32, id);
    failed |= check(SsVabTransCompleted(1) == 1, "SsVabTransCompleted reports complete");
    spu_ram_read(base, back, 64);
    failed |= check(memcmp(back, vab_body, 64) == 0, "two body chunks land back to back at the VAB base");
    SsVabClose(id);
    id2 = SsVabOpenHead(vab_header, -1);
    failed |= check(id2 >= 0 && snd_debug_vab_base(id2) == base, "close then reopen reuses the same sound RAM");
    id = id2;
    SsVabTransBodyPartly(vab_body, 64, id);

    failed |= check(SsUtKeyOnV(5, id, 0, 0, 60, 0, 127, 127) == 5 && spu_debug_pitch(5) == 4096,
                    "key on at the center note gives pitch 4096");
    failed |= check(SsUtKeyOnV(6, id, 0, 0, 72, 0, 127, 127) == 6 && spu_debug_pitch(6) == 8192,
                    "one octave up gives pitch 8192");
    failed |= check(SsUtKeyOnV(7, 9, 0, 0, 60, 0, 127, 127) == -1, "a missing VAB returns -1");

    spu_reset_voices();
    SsSetReservedVoice(4);
    for (i = 0; i < 3; i++) {
        SsUtKeyOnV((short)i, id, 0, 0, 60, 0, 127, 127);
    }
    failed |= check(SsUtKeyOn(id, 1, 0, 60, 0, 127, 127) == 3, "SsUtKeyOn picks the free voice below the reserved count");
    SsSetReservedVoice(24);
    SsVabClose(id);
    spu_reset_voices();
    return failed;
}

static int check_libspu(void) {
    static const uint8_t looping[2] = {0x04, 0x03};
    static const uint8_t one_shot[2] = {0x00, 0x01};
    char status[24];
    uint8_t data[16];
    uint8_t back[16];
    int failed;

    put_blocks(TEST_ADDR, one_shot, 2, 0, 3);
    put_blocks(TEST_ADDR + 0x100, looping, 2, 0, 3);
    spu_voice_start(1, TEST_ADDR + 0x100, SPU_PITCH_ONE, ADSR1_FAST, ADSR2_HOLD_FAST, 0x3FFF, 0x3FFF);
    spu_voice_start(2, TEST_ADDR + 0x100, SPU_PITCH_ONE, ADSR1_FAST, ADSR2_HOLD_SLOW, 0x3FFF, 0x3FFF);
    spu_voice_start(3, TEST_ADDR, SPU_PITCH_ONE, ADSR1_FAST, ADSR2_HOLD_FAST, 0x3FFF, 0x3FFF);
    spu_mix(mix_buf, 100);
    spu_voice_release(2);
    SpuGetAllKeysStatus(status);
    failed = check(status[0] == 0 && status[1] == 1 && status[2] == 2 && status[3] == 3,
                   "SpuGetAllKeysStatus reports off 0, on 1, released 2, ended 3");
    failed |= check(SpuGetKeyStatus(1u << 2) == 2, "SpuGetKeyStatus reports the masked voice");

    memset(data, 0x5A, sizeof(data));
    SpuSetTransferStartAddr(0x2000);
    SpuWrite(data, sizeof(data));
    spu_ram_read(0x2000, back, sizeof(back));
    failed |= check(memcmp(data, back, sizeof(data)) == 0, "SpuWrite after SpuSetTransferStartAddr(0x2000) lands there");
    spu_reset_voices();
    return failed;
}

/* With HMBTN_AUDIO_DUMP set, play a short tone through the running device. */
static void play_dump_tone(void) {
    static const uint8_t looping[2] = {0x04, 0x03};

    if (!spu_audio_is_open() || getenv("HMBTN_AUDIO_DUMP") == NULL) {
        return;
    }
    put_blocks(TEST_ADDR + 0x300, looping, 2, 4, 7);
    spu_voice_start(0, TEST_ADDR + 0x300, 2048, ADSR1_FAST, ADSR2_HOLD_FAST, 0x1000, 0x1000);
    SDL_Delay(300);
    spu_voice_release(0);
    SDL_Delay(50);
    spu_reset_voices();
    SDL_Log("audio check: played the dump tone");
}

int audio_self_check(void) {
    int failed;

    spu_audio_pause(1);
    spu_set_master(0x3FFF, 0x3FFF);
    failed = check_core();
    failed |= check_libsnd();
    failed |= check_libspu();
    spu_audio_pause(0);
    play_dump_tone();
    return failed;
}
