/* Memory card image checks, run at startup when HMBTN_SELFTEST=1. They work on a
 * temporary image in the executable's folder and never open the real card. */

#include <libmcrd.h>
#include <libmcx.h>
#include <memcard.h>

#include <SDL.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DATA_SIZE (6 * MEMCARD_BLOCK_SIZE)

static char test_path[1024];
static uint8_t *file_buf;
static uint8_t *real_buf;
static uint8_t *data_in;
static uint8_t *data_out;

static int check(int ok, const char *what) {
    SDL_Log("memcard check %s: %s", ok ? "ok" : "FAILED", what);
    return ok ? 0 : 1;
}

static long file_size(const char *path) {
    FILE *f;
    long size;

    f = fopen(path, "rb");
    if (f == NULL) {
        return -1;
    }
    fseek(f, 0, SEEK_END);
    size = ftell(f);
    fclose(f);
    return size;
}

static long read_file(const char *path, uint8_t *buf, long max) {
    FILE *f;
    long n;

    f = fopen(path, "rb");
    if (f == NULL) {
        return -1;
    }
    n = (long)fread(buf, 1, (size_t)max, f);
    fclose(f);
    return n;
}

static void write_file(const char *path, const uint8_t *buf, long size) {
    FILE *f;

    f = fopen(path, "wb");
    if (f != NULL) {
        fwrite(buf, 1, (size_t)size, f);
        fclose(f);
    }
}

static int sealed(const uint8_t *f) {
    uint8_t x;
    int i;

    x = 0;
    for (i = 0; i < MEMCARD_FRAME_SIZE - 1; i++) {
        x ^= f[i];
    }
    return x == f[MEMCARD_FRAME_SIZE - 1];
}

static const uint8_t *dir_frame(int block) {
    return memcard_image() + block * MEMCARD_FRAME_SIZE;
}

static int dir_next(int block) {
    return dir_frame(block)[8] | (dir_frame(block)[9] << 8);
}

/* Missing, damaged, and unformatted images. */
static int check_open(void) {
    const uint8_t *img;
    int failures;
    int frames_ok;
    int free_ok;
    int i;

    failures = 0;
    remove(test_path);
    failures += check(memcard_open(test_path) == MEMCARD_READY, "a missing image opens ready");
    failures += check(file_size(test_path) == MEMCARD_SIZE, "the created image is 131072 bytes");
    img = memcard_image();
    failures += check(img[0] == 'M' && img[1] == 'C', "the image starts with MC");
    frames_ok = sealed(dir_frame(0)) && sealed(dir_frame(63)) && memcmp(dir_frame(0), dir_frame(63), 128) == 0;
    free_ok = 1;
    for (i = 1; i <= 35; i++) {
        frames_ok = frames_ok && sealed(dir_frame(i));
    }
    for (i = 1; i <= MEMCARD_DATA_BLOCKS; i++) {
        free_ok = free_ok && dir_frame(i)[0] == 0xA0 && dir_next(i) == 0xFFFF;
    }
    failures += check(frames_ok, "header, directory, and broken-sector frames have valid checksums");
    failures += check(free_ok && memcard_free_blocks() == 15, "15 free directory entries");
    read_file(test_path, file_buf, MEMCARD_SIZE);
    failures += check(memcmp(file_buf, img, MEMCARD_SIZE) == 0, "the file holds the image");

    memset(data_in, 0x5A, 1000);
    write_file(test_path, data_in, 1000);
    failures += check(memcard_open(test_path) == MEMCARD_INVALID, "a 1000-byte file opens invalid");
    failures += check(memcard_format() == MEMCARD_NOT_READY, "an invalid card cannot be formatted");
    failures += check(read_file(test_path, data_out, DATA_SIZE) == 1000 && memcmp(data_in, data_out, 1000) == 0,
                      "the 1000-byte file is unchanged");

    memset(file_buf, 0, MEMCARD_SIZE);
    write_file(test_path, file_buf, MEMCARD_SIZE);
    failures += check(memcard_open(test_path) == MEMCARD_UNFORMATTED, "a zeroed file opens unformatted");
    failures += check(memcard_format() == MEMCARD_OK && memcard_state() == MEMCARD_READY, "formatting makes it ready");
    failures += check(memcard_open(test_path) == MEMCARD_READY && memcard_free_blocks() == 15,
                      "the formatted card reopens ready with 15 free blocks");
    failures += check(memcard_unformat() == MEMCARD_OK && memcard_open(test_path) == MEMCARD_UNFORMATTED,
                      "unformatting makes it report not formatted");
    return failures;
}

/* Directory operations and data along the block chain. */
static int check_files(void) {
    MemcardEntry entries[MEMCARD_DATA_BLOCKS];
    int failures;
    int count;
    int sizes_ok;
    int i;

    failures = 0;
    remove(test_path);
    memcard_open(test_path);
    failures += check(memcard_create("BASLUS-01115-FARM0", 4) == MEMCARD_OK, "create FARM0 with 4 blocks");
    failures += check(dir_frame(1)[0] == 0x51 && dir_frame(2)[0] == 0x52 && dir_frame(3)[0] == 0x52 &&
                          dir_frame(4)[0] == 0x53,
                      "FARM0 entries are first, middle, middle, last");
    failures += check(dir_frame(1)[4] == 0x00 && dir_frame(1)[5] == 0x80 && dir_frame(1)[6] == 0,
                      "FARM0 size is 32768");
    failures += check(dir_next(1) == 1 && dir_next(2) == 2 && dir_next(3) == 3 && dir_next(4) == 0xFFFF,
                      "FARM0 links blocks 1->2->3->4");
    failures += check(strcmp((const char *)dir_frame(1) + 10, "BASLUS-01115-FARM0") == 0 && sealed(dir_frame(1)) &&
                          sealed(dir_frame(4)),
                      "FARM0 name and checksums");
    failures += check(memcard_create("BASLUS-01115-FARM0", 4) == MEMCARD_EXISTS, "creating FARM0 again: exists");
    failures += check(memcard_create("BASLUS-01115-FARM1", 4) == MEMCARD_OK &&
                          memcard_create("BASLUS-01115-FARM2", 4) == MEMCARD_OK && memcard_free_blocks() == 3,
                      "two more 4-block files leave 3 free blocks");
    failures += check(memcard_create("BASLUS-01115-FARMX", 4) == MEMCARD_FULL && memcard_free_blocks() == 3,
                      "another 4-block file: block full, directory unchanged");
    failures += check(memcard_delete("BASLUS-01115-FARM1") == MEMCARD_OK && memcard_free_blocks() == 7 &&
                          memcard_find("BASLUS-01115-FARM1") < 0,
                      "deleting FARM1 frees its 4 blocks");
    failures += check(memcard_delete("BASLUS-01115-FARM1") == MEMCARD_NOT_FOUND, "deleting it again: not found");

    count = memcard_list("*", entries, MEMCARD_DATA_BLOCKS);
    sizes_ok = count == 2;
    for (i = 0; i < count; i++) {
        sizes_ok = sizes_ok && entries[i].size == 32768 && entries[i].blocks == 4;
    }
    failures += check(sizes_ok && strcmp(entries[0].name, "BASLUS-01115-FARM0") == 0 &&
                          strcmp(entries[1].name, "BASLUS-01115-FARM2") == 0,
                      "listing * returns FARM0 and FARM2 with 32768 bytes each");
    failures += check(memcard_list("BASLUS-01115-FARM2", entries, 1) == 1 && entries[0].first_block == 9 &&
                          memcard_list("BASLUS-01115-FARM1", entries, 1) == 0,
                      "listing an exact name");

    for (i = 0; i < (int)DATA_SIZE; i++) {
        data_in[i] = (uint8_t)(i * 7 + (i >> 8));
    }
    failures += check(memcard_write("BASLUS-01115-FARM0", data_in, 0, 0x8000) == MEMCARD_OK, "write 32768 bytes");
    failures += check(memcard_create("SPLIT", 6) == MEMCARD_OK && memcard_find("SPLIT") == 5 && dir_next(8) == 12,
                      "a 6-block file chains blocks 5-8 to 13-14");
    failures += check(memcard_write("SPLIT", data_in, 0, DATA_SIZE) == MEMCARD_OK, "write across the split chain");
    memcard_open(test_path);
    memset(data_out, 0, DATA_SIZE);
    failures += check(memcard_read("BASLUS-01115-FARM0", data_out, 0, 0x8000) == MEMCARD_OK &&
                          memcmp(data_in, data_out, 0x8000) == 0,
                      "FARM0 reads back equal after reopening");
    memset(data_out, 0, DATA_SIZE);
    failures += check(memcard_read("SPLIT", data_out, 0, DATA_SIZE) == MEMCARD_OK &&
                          memcmp(data_in, data_out, DATA_SIZE) == 0 &&
                          memcmp(memcard_image() + 13 * MEMCARD_BLOCK_SIZE, data_in + 4 * MEMCARD_BLOCK_SIZE, 64) == 0,
                      "the split file reads back equal, block 5 of it at card block 13");
    memset(data_out, 0, 128);
    failures += check(memcard_read("BASLUS-01115-FARM0", data_out, 0x200, 0x80) == MEMCARD_OK &&
                          memcmp(data_out, data_in + 0x200, 0x80) == 0,
                      "128 bytes at offset 0x200");
    failures += check(memcard_write("BASLUS-01115-FARM0", data_in, 64, 128) == MEMCARD_RANGE, "a write at offset 64: range");
    failures += check(memcard_read("BASLUS-01115-FARM0", data_out, 0x7F80, 0x100) == MEMCARD_RANGE,
                      "a read past the file: range");
    failures += check(memcard_read("NOPE", data_out, 0, 128) == MEMCARD_NOT_FOUND, "a missing file: not found");
    return failures;
}

static int saw_new_card;
static long callback_cmd;
static long callback_result;

static void on_card(long cmd, long result) {
    callback_cmd = cmd;
    callback_result = result;
}

/* MemCardSync(1) once: 1 with (cmd, result), or -1. */
static int sync_is(long want_cmd, long want_result) {
    long cmd;
    long result;

    cmd = -1;
    result = -1;
    if (MemCardSync(1, &cmd, &result) != 1) {
        return 0;
    }
    saw_new_card |= result == McErrNewCard;
    return cmd == want_cmd && result == want_result;
}

static int sync_idle(void) {
    long cmd;
    long result;

    return MemCardSync(1, &cmd, &result) == -1;
}

static int mcx_is(long want_cmd, long want_result) {
    long cmd;
    long result;

    return McxSync(0, &cmd, &result) == 1 && cmd == want_cmd && result == want_result;
}

/* The libmcrd and libmcx calls the game makes, on the temporary image. */
static int check_libraries(void) {
    struct DIRENTRY dir[MEMCARD_DATA_BLOCKS];
    char farm0[] = "BASLUS-01115-FARM0";
    char farm1[] = "BASLUS-01115-FARM1";
    char all[] = "*";
    long files;
    int failures;
    int i;

    failures = 0;
    saw_new_card = 0;
    remove(test_path);
    memcard_open(test_path);
    while (!sync_idle()) {
    }
    failures += check(MemCardAccept(0x00) == 1 && sync_is(McFuncAccept, McErrNone) && sync_idle(),
                      "Accept(0): 1, then Sync gives (2, none) once, then -1");
    MemCardExist(0x10);
    failures += check(sync_is(McFuncExist, McErrCardNotExist), "Exist(0x10): no card");
    MemCardAccept(0x10);
    failures += check(sync_is(McFuncAccept, McErrCardNotExist), "Accept(0x10): no card");
    failures += check(MemCardCreateFile(0x00, farm0, 4) == McErrNone && MemCardCreateFile(0x00, farm0, 4) == McErrAlreadyExist,
                      "CreateFile: none, then already exists");
    failures += check(MemCardCreateFile(0x10, farm1, 4) == McErrCardNotExist, "CreateFile on slot 2: no card");

    for (i = 0; i < 0x8000; i++) {
        data_in[i] = (uint8_t)(i ^ (i >> 7));
    }
    MemCardWriteFile(0x00, farm0, (u_long *)data_in, 0, 0x8000);
    failures += check(sync_is(McFuncWriteFile, McErrNone) && sync_idle(), "WriteFile then two polls: (4, none), then -1");
    failures += check(MemCardCallback(on_card) == NULL, "no callback registered at first");
    memset(data_out, 0, 0x8000);
    MemCardReadFile(0x00, farm0, (u_long *)data_out, 0, 0x8000);
    failures += check(callback_cmd == McFuncReadFile && callback_result == McErrNone && memcmp(data_in, data_out, 0x8000) == 0,
                      "ReadFile: the callback gets (3, none) and the data matches");
    MemCardWriteFile(0x00, farm0, (u_long *)data_in, 0, 0x8000);
    failures += check(callback_cmd == McFuncWriteFile && callback_result == McErrNone, "the callback gets (4, none)");
    failures += check(MemCardCallback(NULL) == on_card, "unregistering returns the callback");
    failures += check(sync_is(McFuncWriteFile, McErrNone), "Sync still reports the write");

    MemCardReadFile(0x00, farm1, (u_long *)data_out, 0, 128);
    failures += check(sync_is(McFuncReadFile, McErrFileNotExist), "ReadFile of a missing file: file not exist");
    MemCardWriteFile(0x00, farm0, (u_long *)data_in, 64, 128);
    failures += check(sync_is(McFuncWriteFile, McErrCardInvalid), "WriteFile at offset 64: invalid");

    memset(dir, 0, sizeof(dir));
    files = -1;
    failures += check(MemCardGetDirentry(0x00, all, dir, &files, 0, MEMCARD_DATA_BLOCKS) == McErrNone && files == 1 &&
                          dir[0].size == 32768 && strncmp(dir[0].name, farm0, 20) == 0,
                      "GetDirentry(*): FARM0 with 32768 bytes");
    files = -1;
    failures += check(MemCardGetDirentry(0x00, farm1, dir, &files, 0, 1) == McErrNone && files == 0,
                      "GetDirentry of a missing save: none, 0 entries");

    failures += check(MemCardOpen(0x00, farm0, 0) == McErrNone, "Open FARM0");
    memset(data_out, 0, 128);
    MemCardReadData((u_long *)data_out, 0x200, 0x80);
    failures += check(sync_is(McFuncReadData, McErrNone) && memcmp(data_out, data_in + 0x200, 0x80) == 0,
                      "ReadData at 0x200: (5, none) and the header frame");
    MemCardClose();
    failures += check(MemCardDeleteFile(0x00, farm0) == McErrNone && MemCardDeleteFile(0x00, farm0) == McErrFileNotExist,
                      "DeleteFile: none, then file not exist");

    failures += check(MemCardUnformat(0x00) == McErrNone, "Unformat");
    MemCardAccept(0x00);
    failures += check(sync_is(McFuncAccept, McErrNotFormat), "Accept on an unformatted card: not formatted");
    MemCardExist(0x00);
    failures += check(sync_is(McFuncExist, McErrNone), "Exist on an unformatted card: none");
    failures += check(McxCardType(0x00) == 1 && mcx_is(McxFuncCardType, McxErrInvalid),
                      "McxCardType(0) on an unformatted card: invalid");
    failures += check(MemCardFormat(0x00) == McErrNone, "Format");
    MemCardAccept(0x00);
    failures += check(sync_is(McFuncAccept, McErrNone), "Accept after Format: none");

    failures += check(McxCardType(0x00) == 1 && mcx_is(McxFuncCardType, McxErrInvalid) && mcx_is(0, McxErrNoCard),
                      "McxCardType(0): 1, McxSync gives invalid once, then no card");
    failures += check(McxCardType(0x10) == 1 && mcx_is(McxFuncCardType, McxErrNoCard), "McxCardType(0x10): no card");
    failures += check(!saw_new_card, "no call reported a new card");
    return failures;
}

int memcard_self_check(void) {
    char real_path[1024];
    long real_size;
    long real_read;
    int failures;
    char *base;
    char tmp[1100];

    file_buf = (uint8_t *)malloc(MEMCARD_SIZE);
    real_buf = (uint8_t *)malloc(MEMCARD_SIZE);
    data_in = (uint8_t *)malloc(DATA_SIZE);
    data_out = (uint8_t *)malloc(DATA_SIZE);
    if (file_buf == NULL || real_buf == NULL || data_in == NULL || data_out == NULL) {
        return check(0, "allocating the check buffers");
    }
    base = SDL_GetBasePath();
    snprintf(test_path, sizeof(test_path), "%smemcard_selftest.mcr", base != NULL ? base : "");
    SDL_free(base);
    snprintf(real_path, sizeof(real_path), "%s", memcard_default_path());
    real_size = file_size(real_path);
    real_read = real_size == MEMCARD_SIZE ? read_file(real_path, real_buf, MEMCARD_SIZE) : -1;

    failures = check_open();
    failures += check_files();
    failures += check_libraries();

    memcard_close();
    remove(test_path);
    snprintf(tmp, sizeof(tmp), "%s.tmp", test_path);
    failures += check(file_size(test_path) < 0 && file_size(tmp) < 0, "the temporary image is deleted");
    failures += check(file_size(real_path) == real_size &&
                          (real_read != MEMCARD_SIZE || (read_file(real_path, file_buf, MEMCARD_SIZE) == MEMCARD_SIZE &&
                                                         memcmp(file_buf, real_buf, MEMCARD_SIZE) == 0)),
                      "the real card file is untouched");
    if (failures != 0) {
        SDL_Log("memcard check: %d failed", failures);
    }
    free(file_buf);
    free(real_buf);
    free(data_in);
    free(data_out);
    return failures;
}
