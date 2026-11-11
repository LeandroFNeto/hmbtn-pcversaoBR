/* One-row picture upload. The game file is not edited: texture_pack_load_at is the
 * public entry texture_pack_upload already uses for a 14x1 LoadImage. */

#include <cd_drive.h>
#include <texture_pack.h>
#include <vram.h>

#include <SDL.h>

#include <stdio.h>
#include <string.h>

#define ITEMPIC_X 800
#define ITEMPIC_Y 480
#define ITEMPIC_TEXEL 0x7FFF
#define ITEMPIC_BASKET 0x064
#define ITEMPIC_SECTOR (0x3C19000u + (ITEMPIC_BASKET << 11))
#define ITEMPIC_SECTOR_BYTES 0x800u

static int check(int ok, const char *what) {
    SDL_Log("itempic check %s: %s", ok ? "ok" : "FAILED", what);
    return ok ? 0 : 1;
}

/* type 0x0D, one block, flag bits 2-3 clear, one halfword, one row, a 2-byte literal. */
static void fill_row(unsigned char *pack, unsigned texel) {
    memset(pack, 0, 20);
    pack[0] = 0x0D;
    pack[2] = 1;
    pack[4] = 3;
    pack[12] = 1;
    pack[14] = 1;
    pack[16] = 0xFE;
    pack[17] = (unsigned char)(texel & 0xFF);
    pack[18] = (unsigned char)(texel >> 8);
}

static unsigned read_word(int x, int y) {
    return vram[y][x];
}

/* The first word is the magic. Every byte after it is the rest of the sector the
 * upload can consume. `image zero` is that rest being all 0x00. */
static int read_sector(unsigned *magic, int *image_zero) {
    const char *host;
    const char *disc;
    FILE *file;
    unsigned char buf[ITEMPIC_SECTOR_BYTES];
    uint32_t lba;
    uint32_t size;
    unsigned i;
    int n;
    size_t got;

    host = NULL;
    n = cd_drive_extent_count();
    for (i = 0; i < (unsigned)n; i++) {
        if (cd_drive_extent((int)i, &disc, &lba, &size) != 0) {
            continue;
        }
        if (strcmp(disc, "A_FILE.BIN") == 0) {
            host = cd_drive_host_path((int)i);
            break;
        }
    }
    if (host == NULL) {
        return -1;
    }
    file = fopen(host, "rb");
    if (file == NULL) {
        return -1;
    }
    if (fseek(file, (long)ITEMPIC_SECTOR, SEEK_SET) != 0) {
        fclose(file);
        return -1;
    }
    got = fread(buf, 1, sizeof buf, file);
    fclose(file);
    if (got != sizeof buf) {
        return -1;
    }
    *magic = (unsigned)buf[0] | ((unsigned)buf[1] << 8) | ((unsigned)buf[2] << 16) | ((unsigned)buf[3] << 24);
    *image_zero = 1;
    for (i = 4; i < sizeof buf; i++) {
        if (buf[i] != 0) {
            *image_zero = 0;
            break;
        }
    }
    return 0;
}

int item_picture_self_check(void) {
    unsigned char pack[20];
    unsigned word;
    int failed;

    failed = 0;
    fill_row(pack, ITEMPIC_TEXEL);
    texture_pack_load_at(pack, ITEMPIC_X, ITEMPIC_Y, 0, 0);
    word = read_word(ITEMPIC_X, ITEMPIC_Y);
    failed |= check(word != 0, "nonzero texel stored");
    SDL_Log("itempic check word 0x%04x", word);

    fill_row(pack, 0);
    texture_pack_load_at(pack, ITEMPIC_X + 1, ITEMPIC_Y, 0, 0);
    word = read_word(ITEMPIC_X + 1, ITEMPIC_Y);
    failed |= check(word == 0, "zero texel stored");
    SDL_Log("itempic check word 0x%04x", word);

    {
        unsigned magic;
        int image_zero;

        if (read_sector(&magic, &image_zero) != 0) {
            SDL_Log("itempic check FAILED: sector unread");
            return 1;
        }
        SDL_Log("itempic: id 0x%03x magic 0x%08x image %s", ITEMPIC_BASKET, magic,
                image_zero ? "zero" : "nonzero");
    }
    return failed;
}
