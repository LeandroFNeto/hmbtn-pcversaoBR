/* CD drive checks against the extracted files, run at startup when HMBTN_SELFTEST=1. */

#include <cd_drive.h>
#include <libcd.h>

#include <SDL.h>

#include <stdio.h>
#include <string.h>

int cd_debug_seek_lba(void);

static unsigned char sector_buf[3 * 2048];
static unsigned char file_buf[2048];
static int callback_calls;
static u_char callback_status;

static int check(int ok, const char *what) {
    SDL_Log("cd check %s: %s", ok ? "ok" : "FAILED", what);
    return ok ? 0 : 1;
}

static int find_extent(const char *disc_path) {
    const char *path;
    uint32_t lba;
    uint32_t sectors;
    int i;

    for (i = 0; i < cd_drive_extent_count(); i++) {
        if (cd_drive_extent(i, &path, &lba, &sectors) == 0 && strcmp(path, disc_path) == 0) {
            return i;
        }
    }
    return -1;
}

/* `count` bytes of the extracted file at `offset`, zero-padded to 2048. */
static long read_file(int extent, long offset, unsigned char *dst) {
    FILE *f;
    long got;

    memset(dst, 0, 2048);
    f = fopen(cd_drive_host_path(extent), "rb");
    if (f == NULL) {
        return -1;
    }
    fseek(f, offset, SEEK_SET);
    got = (long)fread(dst, 1, 2048, f);
    fclose(f);
    return got;
}

static void count_callback(u_char status, u_char *result) {
    (void)result;
    callback_calls++;
    callback_status = status;
}

int cd_drive_self_check(void) {
    CdlFILE file;
    CdlLOC loc;
    const char *path;
    uint32_t lba;
    uint32_t sectors;
    int hdt;
    int bin;
    int lowest_ok;
    int failed;
    int i;

    failed = check(cd_drive_is_open(), "drive is open");
    if (!cd_drive_is_open()) {
        return failed;
    }

    hdt = find_extent("A_FILE.HDT");
    bin = find_extent("A_FILE.BIN");
    failed |= check(hdt >= 0 && bin >= 0 && find_extent("STR/01.XA") >= 0,
                    "table holds A_FILE.BIN, A_FILE.HDT and STR/01.XA");
    lowest_ok = 1;
    for (i = 0; i < cd_drive_extent_count(); i++) {
        cd_drive_extent(i, &path, &lba, &sectors);
        if (lba < CD_DRIVE_FIRST_LBA) {
            lowest_ok = 0;
        }
    }
    failed |= check(lowest_ok, "no extent starts below sector 24");
    if (hdt < 0 || bin < 0) {
        return failed;
    }

    failed |= check(CdSearchFile(&file, "\\A_FILE.BIN;1") == &file, "\\A_FILE.BIN;1 returns its structure");
    cd_drive_extent(bin, &path, &lba, &sectors);
    failed |= check(CdPosToInt(&file.pos) == (int)lba, "A_FILE.BIN position is its first sector");
    {
        FILE *f = fopen(cd_drive_host_path(bin), "rb");
        long length = -1;

        if (f != NULL) {
            fseek(f, 0, SEEK_END);
            length = ftell(f);
            fclose(f);
        }
        failed |= check((long)file.size == length, "A_FILE.BIN size is the file length");
    }
    failed |= check(CdSearchFile(&file, "\\str\\01.xa") == &file, "\\str\\01.xa is found");
    failed |= check(CdSearchFile(&file, "\\NO\\SUCH.BIN;1") == NULL, "an unknown name returns null");

    CdIntToPos(0, &loc);
    failed |= check(loc.minute == 0x00 && loc.second == 0x02 && loc.sector == 0x00, "sector 0 is 00:02:00");
    failed |= check(CdPosToInt(CdIntToPos(12345, &loc)) == 12345, "sector 12345 round-trips");

    /* A_FILE.HDT: one sector at its position equals the file, zero-padded. */
    CdSearchFile(&file, "\\A_FILE.HDT;1");
    CdControl(CdlSetloc, (u_char *)&file.pos, 0);
    memset(sector_buf, 0xAA, sizeof(sector_buf));
    failed |= check(CdRead(1, (u_long *)sector_buf, 0x80) == 1, "CdRead of A_FILE.HDT succeeds");
    read_file(hdt, 0, file_buf);
    failed |= check(memcmp(sector_buf, file_buf, 2048) == 0, "A_FILE.HDT sector equals the file, zero-padded");
    failed |= check(cd_debug_seek_lba() == CdPosToInt(&file.pos) + 1, "the seek advances by one sector");
    failed |= check(CdReadSync(1, 0) == 0, "CdReadSync(1, 0) reports done");
    failed |= check(CdSync(1, 0) == CdlComplete, "CdSync reports complete");

    /* A_FILE.HDT is 132 bytes: the rest of its sector is past the end. Sector 5 is in no file. */
    {
        int zero_tail = 1;
        int zero_gap = 1;

        for (i = 132; i < 2048; i++) {
            if (sector_buf[i] != 0) {
                zero_tail = 0;
            }
        }
        failed |= check(zero_tail, "bytes past A_FILE.HDT's end read as zeros");
        CdIntToPos(5, &loc);
        CdControl(CdlSetloc, (u_char *)&loc, 0);
        memset(sector_buf, 0xAA, 2 * 2048);
        CdRead(2, (u_long *)sector_buf, 0x80);
        for (i = 0; i < 2 * 2048; i++) {
            if (sector_buf[i] != 0) {
                zero_gap = 0;
            }
        }
        failed |= check(zero_gap, "a sector no file holds reads as zeros");
    }

    /* Sector k = 3 of A_FILE.BIN, read with a callback registered. */
    cd_drive_extent(bin, &path, &lba, &sectors);
    CdIntToPos((int)lba + 3, &loc);
    CdControl(CdlSetloc, (u_char *)&loc, 0);
    callback_calls = 0;
    CdReadCallback(count_callback);
    CdRead(1, (u_long *)sector_buf, 0x80);
    CdReadCallback(NULL);
    read_file(bin, 3 * 2048, file_buf);
    failed |= check(memcmp(sector_buf, file_buf, 2048) == 0, "A_FILE.BIN sector 3 equals bytes 6144..8191");
    failed |= check(callback_calls == 1 && callback_status == CdlComplete,
                    "the read callback runs once with CdlComplete");

    return failed;
}
