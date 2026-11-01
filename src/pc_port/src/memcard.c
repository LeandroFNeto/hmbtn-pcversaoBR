/* The memory card image. Layout of block 0 (frames of 128 bytes):
 *   0       "MC", checksum
 *   1-15    directory entry of data block 1-15
 *   16-35   broken-sector list (none)
 *   63      copy of frame 0
 * Directory entry: u32 state at 0x00, u32 size at 0x04 (first block), u16 next block
 * index at 0x08 (0xFFFF ends the chain), name at 0x0A, XOR checksum at 0x7F. */

#include <memcard.h>

#include <SDL.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STATE_FREE 0xA0
#define STATE_FIRST 0x51
#define STATE_MIDDLE 0x52
#define STATE_LAST 0x53
#define NEXT_NONE 0xFFFF
#define ENTRY_SIZE 0x04
#define ENTRY_NEXT 0x08
#define ENTRY_NAME 0x0A
#define BROKEN_FIRST 16
#define BROKEN_LAST 35
#define WRITE_TEST_FRAME 63

/* On the heap: the executable's image must end below 0x1000000 (see host_image_check.cmake). */
static uint8_t *image;
static uint8_t *backup;
static char card_path[1024];
static MemcardState state = MEMCARD_ABSENT;
static int is_open;

static uint8_t *frame(int n) {
    return image + n * MEMCARD_FRAME_SIZE;
}

static void seal(uint8_t *f) {
    uint8_t x;
    int i;

    x = 0;
    for (i = 0; i < MEMCARD_FRAME_SIZE - 1; i++) {
        x ^= f[i];
    }
    f[MEMCARD_FRAME_SIZE - 1] = x;
}

static uint32_t get32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void put32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static void put16(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}

static int entry_state(int block) {
    return frame(block)[0];
}

static int entry_next(int block) {
    const uint8_t *f = frame(block);
    return f[ENTRY_NEXT] | (f[ENTRY_NEXT + 1] << 8);
}

static void free_entry(int block) {
    uint8_t *f = frame(block);

    memset(f, 0, MEMCARD_FRAME_SIZE);
    put32(f, STATE_FREE);
    put16(f + ENTRY_NEXT, NEXT_NONE);
    seal(f);
}

static void format_block0(void) {
    uint8_t *f;
    int i;

    memset(image, 0, MEMCARD_BLOCK_SIZE);
    f = frame(0);
    f[0] = 'M';
    f[1] = 'C';
    seal(f);
    for (i = 1; i <= MEMCARD_DATA_BLOCKS; i++) {
        free_entry(i);
    }
    for (i = BROKEN_FIRST; i <= BROKEN_LAST; i++) {
        f = frame(i);
        put32(f, 0xFFFFFFFF);
        put16(f + ENTRY_NEXT, NEXT_NONE);
        seal(f);
    }
    memcpy(frame(WRITE_TEST_FRAME), frame(0), MEMCARD_FRAME_SIZE);
}

/* Writes the image to <path>.tmp and replaces the card file with it. */
static MemcardResult commit(void) {
    char tmp[sizeof(card_path) + 8];
    unsigned long err;
    FILE *f;
    size_t written;

    snprintf(tmp, sizeof(tmp), "%s.tmp", card_path);
    f = fopen(tmp, "wb");
    if (f == NULL) {
        SDL_Log("memcard: cannot write %s", tmp);
        return MEMCARD_IO;
    }
    written = fwrite(image, 1, MEMCARD_SIZE, f);
    if (fflush(f) != 0 || written != MEMCARD_SIZE) {
        fclose(f);
        remove(tmp);
        SDL_Log("memcard: short write to %s", tmp);
        return MEMCARD_IO;
    }
    fclose(f);
    err = memcard_replace_file(tmp, card_path);
    if (err != 0) {
        remove(tmp);
        SDL_Log("memcard: cannot replace %s (error %lu)", card_path, err);
        return MEMCARD_IO;
    }
    return MEMCARD_OK;
}

/* Changes go to `image`; a failed commit puts the previous image back. */
static void begin_change(void) {
    memcpy(backup, image, MEMCARD_SIZE);
}

static MemcardResult end_change(void) {
    MemcardResult result;

    result = commit();
    if (result != MEMCARD_OK) {
        memcpy(image, backup, MEMCARD_SIZE);
    }
    return result;
}

const char *memcard_default_path(void) {
    static char path[1024];
    const char *env;
    char *base;

    env = getenv("HMBTN_MEMCARD");
    if (env != NULL && env[0] != '\0') {
        snprintf(path, sizeof(path), "%s", env);
    } else {
        base = SDL_GetBasePath();
        snprintf(path, sizeof(path), "%smcd001.mcr", base != NULL ? base : "");
        SDL_free(base);
    }
    return path;
}

MemcardState memcard_open(const char *path) {
    FILE *f;
    long size;

    memcard_close();
    snprintf(card_path, sizeof(card_path), "%s", path);
    if (image == NULL) {
        image = (uint8_t *)malloc(MEMCARD_SIZE);
        backup = (uint8_t *)malloc(MEMCARD_SIZE);
        if (image == NULL || backup == NULL) {
            SDL_Log("memcard: out of memory");
            return state;
        }
    }
    is_open = 1;
    f = fopen(card_path, "rb");
    if (f == NULL) {
        format_block0();
        memset(image + MEMCARD_BLOCK_SIZE, 0, MEMCARD_SIZE - MEMCARD_BLOCK_SIZE);
        if (commit() != MEMCARD_OK) {
            SDL_Log("memcard: cannot create %s; slot 1 is empty", card_path);
            state = MEMCARD_ABSENT;
            return state;
        }
        SDL_Log("memcard: created %s", card_path);
        state = MEMCARD_READY;
        return state;
    }
    size = -1;
    if (fseek(f, 0, SEEK_END) == 0) {
        size = ftell(f);
    }
    if (size != MEMCARD_SIZE || fseek(f, 0, SEEK_SET) != 0 || fread(image, 1, MEMCARD_SIZE, f) != MEMCARD_SIZE) {
        fclose(f);
        SDL_Log("memcard: %s is not a %d-byte card (%ld bytes); slot 1 is damaged", card_path, MEMCARD_SIZE, size);
        state = MEMCARD_INVALID;
        return state;
    }
    fclose(f);
    if (image[0] != 'M' || image[1] != 'C') {
        SDL_Log("memcard: %s is not formatted", card_path);
        state = MEMCARD_UNFORMATTED;
        return state;
    }
    SDL_Log("memcard: opened %s, %d free blocks", card_path, memcard_free_blocks());
    state = MEMCARD_READY;
    return state;
}

void memcard_close(void) {
    is_open = 0;
    state = MEMCARD_ABSENT;
}

int memcard_is_open(void) {
    return is_open;
}

MemcardState memcard_state(void) {
    return state;
}

const char *memcard_path(void) {
    return card_path;
}

const uint8_t *memcard_image(void) {
    return image;
}

MemcardResult memcard_format(void) {
    MemcardResult result;

    if (state != MEMCARD_READY && state != MEMCARD_UNFORMATTED) {
        return MEMCARD_NOT_READY;
    }
    begin_change();
    format_block0();
    result = end_change();
    if (result == MEMCARD_OK) {
        state = MEMCARD_READY;
    }
    SDL_Log("memcard: format %s: %s", card_path, result == MEMCARD_OK ? "ok" : "failed");
    return result;
}

MemcardResult memcard_unformat(void) {
    MemcardResult result;

    if (state != MEMCARD_READY && state != MEMCARD_UNFORMATTED) {
        return MEMCARD_NOT_READY;
    }
    begin_change();
    memset(frame(0), 0, MEMCARD_FRAME_SIZE);
    memset(frame(WRITE_TEST_FRAME), 0, MEMCARD_FRAME_SIZE);
    result = end_change();
    if (result == MEMCARD_OK) {
        state = MEMCARD_UNFORMATTED;
    }
    SDL_Log("memcard: unformat %s: %s", card_path, result == MEMCARD_OK ? "ok" : "failed");
    return result;
}

static int names_equal(int block, const char *name) {
    return strncmp((const char *)frame(block) + ENTRY_NAME, name, MEMCARD_NAME_MAX) == 0;
}

/* Fills `chain` with the data blocks of the file starting at `first`; returns the count,
 * or 0 for a broken chain. */
static int file_chain(int first, int chain[MEMCARD_DATA_BLOCKS]) {
    int count;
    int block;
    int next;

    count = 0;
    block = first;
    for (;;) {
        if (block < 1 || block > MEMCARD_DATA_BLOCKS || count == MEMCARD_DATA_BLOCKS) {
            return 0;
        }
        chain[count++] = block;
        next = entry_next(block);
        if (next == NEXT_NONE) {
            return count;
        }
        block = next + 1;
    }
}

int memcard_find(const char *name) {
    int block;

    if (state != MEMCARD_READY) {
        return -1;
    }
    for (block = 1; block <= MEMCARD_DATA_BLOCKS; block++) {
        if (entry_state(block) == STATE_FIRST && names_equal(block, name)) {
            return block;
        }
    }
    return -1;
}

int memcard_free_blocks(void) {
    int block;
    int count;

    count = 0;
    for (block = 1; block <= MEMCARD_DATA_BLOCKS; block++) {
        if ((entry_state(block) & 0xF0) == STATE_FREE) {
            count++;
        }
    }
    return count;
}

int memcard_list(const char *pattern, MemcardEntry *out, int max) {
    int chain[MEMCARD_DATA_BLOCKS];
    int block;
    int count;
    int all;

    if (state != MEMCARD_READY) {
        return 0;
    }
    all = strcmp(pattern, "*") == 0;
    count = 0;
    for (block = 1; block <= MEMCARD_DATA_BLOCKS && count < max; block++) {
        if (entry_state(block) != STATE_FIRST || (!all && !names_equal(block, pattern))) {
            continue;
        }
        memcpy(out[count].name, frame(block) + ENTRY_NAME, MEMCARD_NAME_MAX);
        out[count].name[MEMCARD_NAME_MAX] = '\0';
        out[count].first_block = block;
        out[count].blocks = file_chain(block, chain);
        out[count].size = (long)get32(frame(block) + ENTRY_SIZE);
        count++;
    }
    return count;
}

MemcardResult memcard_create(const char *name, int blocks) {
    int chain[MEMCARD_DATA_BLOCKS];
    MemcardResult result;
    uint8_t *f;
    int block;
    int count;
    int i;

    if (state != MEMCARD_READY) {
        return MEMCARD_NOT_READY;
    }
    if (name[0] == '\0' || strlen(name) > MEMCARD_NAME_MAX || blocks < 1 || blocks > MEMCARD_DATA_BLOCKS) {
        return MEMCARD_RANGE;
    }
    if (memcard_find(name) >= 0) {
        return MEMCARD_EXISTS;
    }
    count = 0;
    for (block = 1; block <= MEMCARD_DATA_BLOCKS && count < blocks; block++) {
        if ((entry_state(block) & 0xF0) == STATE_FREE) {
            chain[count++] = block;
        }
    }
    if (count < blocks) {
        return MEMCARD_FULL;
    }
    begin_change();
    for (i = 0; i < blocks; i++) {
        f = frame(chain[i]);
        memset(f, 0, MEMCARD_FRAME_SIZE);
        if (i == 0) {
            put32(f, STATE_FIRST);
            put32(f + ENTRY_SIZE, (uint32_t)blocks * MEMCARD_BLOCK_SIZE);
            memcpy(f + ENTRY_NAME, name, strlen(name));
        } else {
            put32(f, i == blocks - 1 ? STATE_LAST : STATE_MIDDLE);
        }
        put16(f + ENTRY_NEXT, (uint16_t)(i == blocks - 1 ? NEXT_NONE : chain[i + 1] - 1));
        seal(f);
        memset(image + chain[i] * MEMCARD_BLOCK_SIZE, 0, MEMCARD_BLOCK_SIZE);
    }
    result = end_change();
    SDL_Log("memcard: create %s, %d blocks from block %d: %s", name, blocks, chain[0],
            result == MEMCARD_OK ? "ok" : "failed");
    return result;
}

MemcardResult memcard_delete(const char *name) {
    int chain[MEMCARD_DATA_BLOCKS];
    MemcardResult result;
    int count;
    int first;
    int i;

    first = memcard_find(name);
    if (first < 0) {
        return state == MEMCARD_READY ? MEMCARD_NOT_FOUND : MEMCARD_NOT_READY;
    }
    count = file_chain(first, chain);
    begin_change();
    if (count == 0) {
        free_entry(first);
    }
    for (i = 0; i < count; i++) {
        free_entry(chain[i]);
    }
    result = end_change();
    SDL_Log("memcard: delete %s: %s", name, result == MEMCARD_OK ? "ok" : "failed");
    return result;
}

/* Finds the file and checks the range; fills `chain` and returns MEMCARD_OK. */
static MemcardResult locate(const char *name, long ofs, long bytes, int chain[MEMCARD_DATA_BLOCKS]) {
    int first;
    int count;

    if (state != MEMCARD_READY) {
        return MEMCARD_NOT_READY;
    }
    first = memcard_find(name);
    if (first < 0) {
        return MEMCARD_NOT_FOUND;
    }
    count = file_chain(first, chain);
    if (count == 0 || ofs < 0 || bytes < 0 || ofs % MEMCARD_FRAME_SIZE != 0 || bytes % MEMCARD_FRAME_SIZE != 0 ||
        ofs + bytes > (long)count * MEMCARD_BLOCK_SIZE) {
        return MEMCARD_RANGE;
    }
    return MEMCARD_OK;
}

/* Copies between `buf` and the file's bytes [ofs, ofs + bytes), block by block. */
static void copy_range(const int *chain, uint8_t *buf, long ofs, long bytes, int to_card) {
    long pos;
    long in_block;
    long n;
    uint8_t *card;

    pos = ofs;
    while (bytes > 0) {
        in_block = pos % MEMCARD_BLOCK_SIZE;
        n = MEMCARD_BLOCK_SIZE - in_block;
        if (n > bytes) {
            n = bytes;
        }
        card = image + chain[pos / MEMCARD_BLOCK_SIZE] * MEMCARD_BLOCK_SIZE + in_block;
        if (to_card) {
            memcpy(card, buf, (size_t)n);
        } else {
            memcpy(buf, card, (size_t)n);
        }
        buf += n;
        pos += n;
        bytes -= n;
    }
}

MemcardResult memcard_read(const char *name, void *dst, long ofs, long bytes) {
    int chain[MEMCARD_DATA_BLOCKS];
    MemcardResult result;

    result = locate(name, ofs, bytes, chain);
    if (result == MEMCARD_OK) {
        copy_range(chain, (uint8_t *)dst, ofs, bytes, 0);
    }
    return result;
}

MemcardResult memcard_write(const char *name, const void *src, long ofs, long bytes) {
    int chain[MEMCARD_DATA_BLOCKS];
    MemcardResult result;

    result = locate(name, ofs, bytes, chain);
    if (result != MEMCARD_OK) {
        return result;
    }
    begin_change();
    copy_range(chain, (uint8_t *)src, ofs, bytes, 1);
    result = end_change();
    SDL_Log("memcard: write %s, 0x%lX bytes at 0x%lX: %s", name, bytes, ofs, result == MEMCARD_OK ? "ok" : "failed");
    return result;
}
