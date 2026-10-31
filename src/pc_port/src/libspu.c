/* libspu on the host SPU: master volume, key status, and transfers to sound RAM. */

#include <libspu.h>
#include <spu_core.h>

static uint32_t transfer_addr;
static uint32_t voice_addr[SPU_VOICES];

void SpuInit(void) {
    spu_reset_voices();
    transfer_addr = 0;
}

void SpuSetCommonAttr(SpuCommonAttr *attr) {
    static int master_l = 0x3FFF;
    static int master_r = 0x3FFF;

    if (attr->mask & SPU_COMMON_MVOLL) {
        master_l = attr->mvol.left & 0x3FFF;
    }
    if (attr->mask & SPU_COMMON_MVOLR) {
        master_r = attr->mvol.right & 0x3FFF;
    }
    spu_set_master(master_l, master_r);
}

long SpuGetKeyStatus(u_long voice_bit) {
    int i;

    for (i = 0; i < SPU_VOICES; i++) {
        if (voice_bit & (1u << i)) {
            return spu_voice_status(i);
        }
    }
    return SPU_STATUS_OFF;
}

void SpuGetAllKeysStatus(char *status) {
    int i;

    for (i = 0; i < SPU_VOICES; i++) {
        status[i] = (char)spu_voice_status(i);
    }
}

u_long SpuSetTransferStartAddr(u_long addr) {
    if (addr >= SPU_RAM_SIZE) {
        return 0;
    }
    transfer_addr = (uint32_t)addr;
    return addr;
}

u_long SpuWrite(u_char *addr, u_long size) {
    uint32_t n;

    n = spu_ram_write(transfer_addr, addr, (uint32_t)size);
    transfer_addr += n;
    return n;
}

/* Keys voices on at the address last given to SpuSetVoiceStartAddr-style setup
 * (the voice's previous start), or off. */
u_long SpuSetKey(long on_off, u_long voice_bit) {
    int i;

    for (i = 0; i < SPU_VOICES; i++) {
        if (!(voice_bit & (1u << i))) {
            continue;
        }
        if (on_off == SPU_ON) {
            spu_voice_start(i, voice_addr[i], SPU_PITCH_ONE, 0x00FF, 0x1FC0, 0x3FFF, 0x3FFF);
        } else {
            spu_voice_release(i);
        }
    }
    return voice_bit;
}

void spu_debug_set_voice_addr(int voice, uint32_t addr) {
    if (voice >= 0 && voice < SPU_VOICES) {
        voice_addr[voice] = addr;
    }
}
