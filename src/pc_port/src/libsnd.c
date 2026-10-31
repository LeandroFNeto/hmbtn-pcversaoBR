/* libsnd sound effects on the host SPU: VAB banks in sound RAM and the SsUt voice calls.
 * SEQ music lives in seq_player.c; reverb and serial (CD) input are accepted and do nothing. */

#include <libsnd.h>
#include <seq_player.h>
#include <spu_core.h>

#include <SDL.h>

#include <math.h>
#include <stddef.h>
#include <string.h>

#define VAB_MAGIC 0x56414270    /* "pBAV" */
#define VAB_MAX 16
#define VAB_PROGRAMS 128
#define VAB_TONES 16
#define VAB_VAGS 256
#define SPU_RAM_FIRST 0x1010
#define SPU_ALIGN 16

typedef struct {
    int used;
    int programs;
    uint8_t mvol;
    uint8_t pan;
    ProgAtr prog[VAB_PROGRAMS];
    VagAtr tones[VAB_PROGRAMS][VAB_TONES];
    uint32_t vag_addr[VAB_VAGS];
    uint32_t base;
    uint32_t size;
    uint32_t sent;
} Vab;

static Vab vabs[VAB_MAX];
static int reserved_voices = SPU_VOICES;

/* First fit in sound RAM above the reserved area, around the open VABs. */
static int alloc_spu(uint32_t size, uint32_t *base) {
    uint32_t candidate;
    int moved;
    int i;

    candidate = SPU_RAM_FIRST;
    do {
        moved = 0;
        for (i = 0; i < VAB_MAX; i++) {
            if (vabs[i].used && vabs[i].size > 0 && candidate < vabs[i].base + vabs[i].size &&
                vabs[i].base < candidate + size) {
                candidate = (vabs[i].base + vabs[i].size + SPU_ALIGN - 1) & ~(uint32_t)(SPU_ALIGN - 1);
                moved = 1;
            }
        }
    } while (moved);
    if (candidate + size > SPU_RAM_SIZE) {
        return -1;
    }
    *base = candidate;
    return 0;
}

short SsVabOpenHead(u_char *addr, short vabid) {
    VabHdr hdr;
    const u_char *p;
    uint16_t sizes[VAB_VAGS];
    uint32_t total;
    uint32_t offset;
    Vab *v;
    int id;
    int i;

    memcpy(&hdr, addr, sizeof(hdr));
    if ((uint32_t)hdr.form != VAB_MAGIC || hdr.ps > VAB_PROGRAMS) {
        return -1;
    }
    id = -1;
    if (vabid >= 0 && vabid < VAB_MAX && !vabs[vabid].used) {
        id = vabid;
    }
    for (i = 0; i < VAB_MAX && id < 0; i++) {
        if (!vabs[i].used) {
            id = i;
        }
    }
    if (id < 0) {
        return -1;
    }
    p = addr + sizeof(VabHdr);
    memset(&vabs[id], 0, sizeof(vabs[id]));
    v = &vabs[id];
    memcpy(v->prog, p, sizeof(v->prog));
    p += sizeof(v->prog);
    memcpy(v->tones, p, (size_t)hdr.ps * VAB_TONES * sizeof(VagAtr));
    p += (size_t)hdr.ps * VAB_TONES * sizeof(VagAtr);
    memcpy(sizes, p, sizeof(sizes));

    total = 0;
    for (i = 1; i < VAB_VAGS && i <= hdr.vs; i++) {
        total += (uint32_t)sizes[i] << 3;
    }
    if (alloc_spu(total, &v->base) != 0) {
        return -1;
    }
    offset = 0;
    for (i = 1; i < VAB_VAGS; i++) {
        v->vag_addr[i] = v->base + offset;
        offset += (uint32_t)sizes[i] << 3;
    }
    v->programs = hdr.ps;
    v->mvol = hdr.mvol;
    v->pan = hdr.pan;
    v->size = total;
    v->sent = 0;
    v->used = 1;
    if (spu_debug_tracing()) {
        SDL_Log("audio: VAB %d opened, %d programs, %d VAGs, %u bytes at 0x%05X", id, hdr.ps, hdr.vs,
                (unsigned)total, (unsigned)v->base);
    }
    return (short)id;
}

short SsVabTransBodyPartly(u_char *addr, u_long bufsize, short vabid) {
    Vab *v;
    uint32_t n;

    if (vabid < 0 || vabid >= VAB_MAX || !vabs[vabid].used) {
        return -1;
    }
    v = &vabs[vabid];
    n = v->size - v->sent;
    if (n > bufsize) {
        n = (uint32_t)bufsize;
    }
    spu_ram_write(v->base + v->sent, addr, n);
    v->sent += n;
    return vabid;
}

short SsVabTransBody(u_char *addr, short vabid) {
    if (vabid < 0 || vabid >= VAB_MAX || !vabs[vabid].used) {
        return -1;
    }
    return SsVabTransBodyPartly(addr, vabs[vabid].size, vabid);
}

short SsVabTransCompleted(short immediateFlag) {
    (void)immediateFlag;
    return 1;
}

void SsVabClose(short vabid) {
    if (vabid >= 0 && vabid < VAB_MAX) {
        vabs[vabid].used = 0;
    }
}

uint32_t snd_debug_vab_base(short vabid) {
    return vabid >= 0 && vabid < VAB_MAX ? vabs[vabid].base : 0;
}

/* ---- voices ---- */

static int clamp_pan(int pan) {
    return pan < 0 ? 0 : (pan > 127 ? 127 : pan);
}

static const VagAtr *find_tone(short vabId, short prog, short tone) {
    const Vab *v;
    const VagAtr *t;

    if (vabId < 0 || vabId >= VAB_MAX || !vabs[vabId].used) {
        return NULL;
    }
    v = &vabs[vabId];
    if (prog < 0 || prog >= v->programs || tone < 0 || tone >= VAB_TONES) {
        return NULL;
    }
    t = &v->tones[prog][tone];
    return t->vag > 0 && t->vag < VAB_VAGS ? t : NULL;
}

short snd_key_on_ex(short voice, short vabId, short prog, short tone, short note, short fine, short voll,
                    short volr, int pan_offset, int *out_l, int *out_r) {
    const Vab *v;
    const VagAtr *t;
    double semitones;
    double pitch;
    int pan;
    double pan_l;
    double pan_r;
    double gain;
    int vol_l;
    int vol_r;

    t = find_tone(vabId, prog, tone);
    if (voice < 0 || voice >= SPU_VOICES || t == NULL) {
        return -1;
    }
    v = &vabs[vabId];
    semitones = ((note * 128 + fine) - (t->center * 128 + t->shift)) / 128.0;
    pitch = SPU_PITCH_ONE * pow(2.0, semitones / 12.0);
    pan = clamp_pan(t->pan + v->prog[prog].mpan - 64 + pan_offset);
    pan_l = (127 - pan) / 64.0;
    pan_r = pan / 64.0;
    pan_l = pan_l > 1.0 ? 1.0 : pan_l;
    pan_r = pan_r > 1.0 ? 1.0 : pan_r;
    gain = 0x3FFF * (t->vol / 127.0) * (v->prog[prog].mvol / 127.0) * (v->mvol / 127.0);
    vol_l = (int)(gain * (voll / 127.0) * pan_l);
    vol_r = (int)(gain * (volr / 127.0) * pan_r);
    spu_voice_start(voice, v->vag_addr[t->vag], (uint32_t)(pitch + 0.5), t->adsr1, t->adsr2, vol_l, vol_r);
    if (out_l != NULL) {
        *out_l = vol_l;
    }
    if (out_r != NULL) {
        *out_r = vol_r;
    }
    return voice;
}

/* An off or ended voice below the reserved count, else the one there that started first. */
int snd_pick_voice(void) {
    int chosen;
    int i;

    for (i = 0; i < reserved_voices; i++) {
        int status = spu_voice_status(i);

        if (status == SPU_STATUS_OFF || status == SPU_STATUS_ENDED) {
            return i;
        }
    }
    chosen = -1;
    for (i = 0; i < reserved_voices; i++) {
        if (chosen < 0 || spu_voice_start_order(i) < spu_voice_start_order(chosen)) {
            chosen = i;
        }
    }
    return chosen;
}

int snd_program_tones(short vabId, short prog) {
    if (vabId < 0 || vabId >= VAB_MAX || !vabs[vabId].used || prog < 0 || prog >= vabs[vabId].programs) {
        return -1;
    }
    return vabs[vabId].prog[prog].tones > VAB_TONES ? VAB_TONES : vabs[vabId].prog[prog].tones;
}

int snd_tone_range(short vabId, short prog, short tone, int *min, int *max) {
    const VagAtr *t;

    t = find_tone(vabId, prog, tone);
    if (t == NULL) {
        return -1;
    }
    *min = t->min;
    *max = t->max;
    return 0;
}

static short key_on(short voice, short vabId, short prog, short tone, short note, short fine, short voll,
                    short volr) {
    short result;

    result = snd_key_on_ex(voice, vabId, prog, tone, note, fine, voll, volr, 0, NULL, NULL);
    if (result >= 0) {
        seq_voice_taken(result);
        if (spu_debug_tracing()) {
            SDL_Log("audio: key on voice %d vab %d prog %d tone %d note %d pitch %u at dump frame %u", voice, vabId,
                    prog, tone, note, spu_debug_pitch(voice), (unsigned)spu_debug_dump_frames());
        }
    }
    return result;
}

short SsUtKeyOnV(short voice, short vabId, short prog, short tone, short note, short fine, short voll, short volr) {
    return key_on(voice, vabId, prog, tone, note, fine, voll, volr);
}

short SsUtKeyOn(short vabId, short prog, short tone, short note, short fine, short voll, short volr) {
    int chosen;

    chosen = snd_pick_voice();
    if (chosen < 0) {
        return -1;
    }
    return key_on((short)chosen, vabId, prog, tone, note, fine, voll, volr);
}

short SsUtKeyOff(short voice, short vabId, short prog, short tone, short note) {
    (void)vabId;
    (void)prog;
    (void)tone;
    (void)note;
    spu_voice_release(voice);
    return 0;
}

short SsUtKeyOffV(short voice) {
    if (voice < 0 || voice >= SPU_VOICES) {
        return -1;
    }
    spu_voice_release(voice);
    return 0;
}

void SsVoKeyOff(short voice, short mode) {
    (void)mode;
    spu_voice_release(voice & 0xFF);
}

void SsUtAllKeyOff(short mode) {
    int i;

    (void)mode;
    for (i = 0; i < SPU_VOICES; i++) {
        spu_voice_release(i);
    }
}

short SsUtSetVVol(short vc, short voll, short volr) {
    if (vc < 0 || vc >= SPU_VOICES) {
        return -1;
    }
    spu_voice_set_volume(vc, voll * 0x3FFF / 127, volr * 0x3FFF / 127);
    return 0;
}

void SsSetMVol(short voll, short volr) {
    spu_set_master(voll * 0x3FFF / 127, volr * 0x3FFF / 127);
}

char SsSetReservedVoice(char voices) {
    if (voices < 1 || voices > SPU_VOICES) {
        return -1;
    }
    reserved_voices = voices;
    return voices;
}

/* ---- accepted, no effect ---- */

void SsInit(void) {
}

void SsEnd(void) {
}

void SsStart(void) {
}

void SsSetTickMode(long mode) {
    (void)mode;
}

void SsSetTableSize(char *table, short s_max, short t_max) {
    (void)table;
    (void)s_max;
    (void)t_max;
}

void SsSetSerialAttr(char s_num, char attr, char mode) {
    (void)s_num;
    (void)attr;
    (void)mode;
}

void SsSetSerialVol(char s_num, short voll, short volr) {
    (void)s_num;
    (void)voll;
    (void)volr;
}

void SsUtReverbOn(void) {
}

void SsUtReverbOff(void) {
}

short SsUtSetReverbType(short type) {
    (void)type;
    return 0;
}

void SsUtSetReverbDepth(short ldepth, short rdepth) {
    (void)ldepth;
    (void)rdepth;
}

void SsUtSetReverbDelay(short delay) {
    (void)delay;
}

void SsUtSetReverbFeedback(short feedback) {
    (void)feedback;
}

void SsSetMono(void) {
}

void SsSetStereo(void) {
}

void SsSetRVol(short voll, short volr) {
    (void)voll;
    (void)volr;
}
