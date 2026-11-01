#ifndef PC_PORT_MEMCARD_H
#define PC_PORT_MEMCARD_H

/* The virtual memory card in slot 1: a raw 128 KB PS1 card image on disk. Block 0 holds
 * the header and the directory; blocks 1-15 hold file data in 128-byte frames. Every
 * change is written to the file before the call returns. */

#include <stdint.h>

#define MEMCARD_SIZE 131072
#define MEMCARD_BLOCK_SIZE 8192
#define MEMCARD_FRAME_SIZE 128
#define MEMCARD_BLOCKS 16
#define MEMCARD_DATA_BLOCKS 15
#define MEMCARD_NAME_MAX 20

typedef enum {
    MEMCARD_ABSENT,
    MEMCARD_INVALID,
    MEMCARD_UNFORMATTED,
    MEMCARD_READY,
} MemcardState;

typedef enum {
    MEMCARD_OK,
    MEMCARD_NOT_FOUND,
    MEMCARD_EXISTS,
    MEMCARD_FULL,
    MEMCARD_RANGE,
    MEMCARD_IO,
    MEMCARD_NOT_READY,
} MemcardResult;

typedef struct {
    char name[MEMCARD_NAME_MAX + 1];
    int first_block;
    int blocks;
    long size;
} MemcardEntry;

/* HMBTN_MEMCARD, or mcd001.mcr in the executable's folder. */
const char *memcard_default_path(void);

/* Reads the image at `path`; a missing file is created formatted. Returns the state. */
MemcardState memcard_open(const char *path);
void memcard_close(void);
int memcard_is_open(void);
MemcardState memcard_state(void);
const char *memcard_path(void);
const uint8_t *memcard_image(void);

MemcardResult memcard_format(void);
MemcardResult memcard_unformat(void);

/* `pattern` is an exact name or "*". Returns how many entries were filled. */
int memcard_list(const char *pattern, MemcardEntry *out, int max);
/* First data block (1-15) of `name`, or -1. */
int memcard_find(const char *name);
int memcard_free_blocks(void);
MemcardResult memcard_create(const char *name, int blocks);
MemcardResult memcard_delete(const char *name);

/* Byte access to a file's data along its block chain; `ofs` and `bytes` must be
 * multiples of 128 within the file. */
MemcardResult memcard_read(const char *name, void *dst, long ofs, long bytes);
MemcardResult memcard_write(const char *name, const void *src, long ofs, long bytes);

/* Replaces `path` with `tmp` in one step. Returns 0, or the system error code. */
unsigned long memcard_replace_file(const char *tmp, const char *path);

#endif
