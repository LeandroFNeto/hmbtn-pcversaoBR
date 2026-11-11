/* Shipping-bin sample address. The game files are not edited. */

#include <cd_drive.h>
#include <texture_pack.h>
#include <vram.h>

#include <SDL.h>

#include <stdio.h>
#include <string.h>

#define BIN_PAGE_X 320
#define BIN_PAGE_Y 0
#define BIN_U 246
#define BIN_V 242
#define BIN_DEPTH 4
#define BIN_SAMPLE_X 381
#define BIN_SAMPLE_Y 242
#define BIN_ROW_X 800
#define BIN_ROW_Y 480
#define BIN_SECTOR 0x3C4B000u
#define BIN_SECTOR_BYTES 0x800u
#define BIN_STASH 1024

static int check(int ok, const char *what) {
    SDL_Log("itemaddr check %s: %s", ok ? "ok" : "FAILED", what);
    return ok ? 0 : 1;
}

static void sample_xy(int page_x, int page_y, int u, int v, int depth, int *x, int *y) {
    if (depth <= 4) {
        *x = page_x + u / 4;
    } else if (depth <= 8) {
        *x = page_x + u / 2;
    } else {
        *x = page_x + u;
    }
    *y = page_y + v;
}

static int covers_sample(int x, int y, int w, int h) {
    return x <= BIN_SAMPLE_X && BIN_SAMPLE_X < x + w && y <= BIN_SAMPLE_Y && BIN_SAMPLE_Y < y + h;
}

static int row_nonzero(int x, int y, int w, int h) {
    int row;
    int col;

    for (row = 0; row < h; row++) {
        for (col = 0; col < w; col++) {
            if (vram[(y + row) & 511][(x + col) & 1023] != 0) {
                return 1;
            }
        }
    }
    return 0;
}

static void log_store(int x, int y, int w, int h) {
    SDL_Log("itemaddr: store x %d y %d w %d h %d %s %s %d,%d", x, y, w, h,
            row_nonzero(x, y, w, h) ? "nonzero" : "zero",
            covers_sample(x, y, w, h) ? "covering" : "not covering", BIN_SAMPLE_X, BIN_SAMPLE_Y);
}

/* type 0x0D, one block, one halfword, one row, a 2-byte literal. */
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

static unsigned ru16(const unsigned char *p) {
    return (unsigned)p[0] | ((unsigned)p[1] << 8);
}

static unsigned ru32(const unsigned char *p) {
    return (unsigned)p[0] | ((unsigned)p[1] << 8) | ((unsigned)p[2] << 16) | ((unsigned)p[3] << 24);
}

/* 0 when `off` is one 0x0D pack that fits in the sector, and `end` is the byte after it. */
static int pack_end(const unsigned char *buf, int len, int off, int *end) {
    unsigned count;
    unsigned i;
    unsigned pos;
    unsigned size;
    unsigned w;
    unsigned h;

    if (off < 0 || off + 4 > len || buf[off] != 0x0D) {
        return -1;
    }
    count = ru16(buf + off + 2);
    if (count < 1 || count > 8) {
        return -1;
    }
    pos = (unsigned)off + 4;
    for (i = 0; i < count; i++) {
        if (pos + 12 > (unsigned)len) {
            return -1;
        }
        size = ru32(buf + pos) & 0xFFFFFFu;
        w = ru16(buf + pos + 8);
        h = ru16(buf + pos + 10);
        if (w == 0 || w > 256 || h == 0 || h > 512 || pos + 12 + size > (unsigned)len) {
            return -1;
        }
        pos += ((size + 3u) & ~3u) + 12u;
    }
    *end = (int)pos;
    return 0;
}

/* texture_pack_load_at is called with origin 0,0 and alt 0,0, so the rectangle is the
 * one stored in the block. */
static void placed_rect(const unsigned char *block, int *x, int *y, int *w, int *h) {
    *x = (int)(short)ru16(block + 4);
    *y = (int)(short)ru16(block + 6);
    *w = (int)ru16(block + 8);
    *h = (int)ru16(block + 10);
}

static int read_sector(unsigned char *buf) {
    const char *host;
    const char *disc;
    FILE *file;
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
    if (fseek(file, (long)BIN_SECTOR, SEEK_SET) != 0) {
        fclose(file);
        return -1;
    }
    got = fread(buf, 1, BIN_SECTOR_BYTES, file);
    fclose(file);
    return got == BIN_SECTOR_BYTES ? 0 : -1;
}

/* Upload every 0x0D pack at the coordinates stored in it, log each row, then put the
 * words back. The sample address is not a place this call writes. */
static int upload_picture(void) {
    unsigned char buf[BIN_SECTOR_BYTES];
    unsigned short stash[BIN_STASH];
    int rx[8];
    int ry[8];
    int rw[8];
    int rh[8];
    int stash_at[8];
    int blocks;
    int off;
    int end;
    int pos;
    int count;
    int i;
    int row;
    int col;
    int x;
    int y;
    int w;
    int h;
    int used;
    int packs;
    unsigned before;

    if (read_sector(buf) != 0) {
        SDL_Log("itemaddr check FAILED: sector unread");
        return 1;
    }
    before = vram[BIN_SAMPLE_Y][BIN_SAMPLE_X];
    packs = 0;
    used = 0;
    blocks = 0;
    for (off = 0; off < BIN_SECTOR_BYTES; off++) {
        if (pack_end(buf, BIN_SECTOR_BYTES, off, &end) != 0) {
            continue;
        }
        pos = off + 4;
        count = (int)ru16(buf + off + 2);
        for (i = 0; i < count; i++) {
            unsigned size;

            placed_rect(buf + pos, &x, &y, &w, &h);
            if (blocks >= 8 || used + w * h > BIN_STASH) {
                SDL_Log("itemaddr check FAILED: picture rows do not fit");
                return 1;
            }
            rx[blocks] = x;
            ry[blocks] = y;
            rw[blocks] = w;
            rh[blocks] = h;
            stash_at[blocks] = used;
            for (row = 0; row < h; row++) {
                for (col = 0; col < w; col++) {
                    stash[used++] = vram[(y + row) & 511][(x + col) & 1023];
                }
            }
            blocks++;
            size = ru32(buf + pos) & 0xFFFFFFu;
            pos += (int)(((size + 3u) & ~3u) + 12u);
        }
        texture_pack_load_at(buf + off, 0, 0, 0, 0);
        packs++;
        off = end - 1;
    }
    if (packs == 0) {
        SDL_Log("itemaddr check FAILED: no 0x0D pack");
        return 1;
    }
    for (i = 0; i < blocks; i++) {
        for (row = 0; row < rh[i]; row++) {
            log_store(rx[i], ry[i] + row, rw[i], 1);
        }
    }
    used = 0;
    for (i = 0; i < blocks; i++) {
        used = stash_at[i];
        for (row = 0; row < rh[i]; row++) {
            for (col = 0; col < rw[i]; col++) {
                vram[(ry[i] + row) & 511][(rx[i] + col) & 1023] = stash[used++];
            }
        }
    }
    if (vram[BIN_SAMPLE_Y][BIN_SAMPLE_X] != before) {
        SDL_Log("itemaddr check FAILED: sample word changed");
        return 1;
    }
    return 0;
}

int bin_sample_self_check(void) {
    unsigned char pack[20];
    int x;
    int y;
    int failed;
    unsigned word;

    failed = 0;
    sample_xy(BIN_PAGE_X, BIN_PAGE_Y, BIN_U, BIN_V, BIN_DEPTH, &x, &y);
    SDL_Log("itemaddr: sample x %d y %d", x, y);
    failed |= check(x == BIN_SAMPLE_X && y == BIN_SAMPLE_Y, "sample x 381 y 242");

    fill_row(pack, 0x7FFF);
    texture_pack_load_at(pack, BIN_ROW_X, BIN_ROW_Y, 0, 0);
    log_store(BIN_ROW_X, BIN_ROW_Y, 1, 1);
    word = vram[BIN_ROW_Y][BIN_ROW_X];
    failed |= check(word == 0x7FFF, "nonzero texel stored");
    failed |= check(!covers_sample(BIN_ROW_X, BIN_ROW_Y, 1, 1), "row not covering sample");

    fill_row(pack, 0);
    texture_pack_load_at(pack, BIN_ROW_X + 1, BIN_ROW_Y, 0, 0);
    log_store(BIN_ROW_X + 1, BIN_ROW_Y, 1, 1);
    word = vram[BIN_ROW_Y][BIN_ROW_X + 1];
    failed |= check(word == 0x0000, "zero texel stored");
    SDL_Log("itemaddr check word 0x%04x", word);
    failed |= upload_picture();
    return failed;
}
