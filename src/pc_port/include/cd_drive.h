#ifndef PC_PORT_CD_DRIVE_H
#define PC_PORT_CD_DRIVE_H

#include <stddef.h>
#include <stdint.h>

/* The host CD drive: the files extracted from the disc image, each given a run of
 * 2048-byte sectors. The sector numbers are this layout's, not the original disc's;
 * the game only adds offsets to the positions CdSearchFile returns. */

#define CD_DRIVE_SECTOR 2048u
#define CD_DRIVE_FIRST_LBA 24u

/* Opens `dir`, or HMBTN_DISC_DIR, or disc/ beside the executable when `dir` is NULL.
 * Returns 0, or -1 when the folder cannot be read. Opening again is a no-op. */
int cd_drive_open(const char *dir);
int cd_drive_is_open(void);
const char *cd_drive_dir(void);

/* Looks up a disc name such as "\A_FILE.BIN;1". Returns 0 and fills the first sector
 * and the ISO size in bytes, or -1 when no file has that name. */
int cd_drive_find(const char *disc_name, uint32_t *first_lba, uint32_t *size);

/* Copies `sectors` sectors starting at `lba` into `dst`, zero-filling bytes no Form 1
 * file holds. Returns 0, or -1 after a file read error. */
int cd_drive_read(uint32_t lba, uint32_t sectors, void *dst);

/* The extent table, for checks: upper-case relative path, first sector, sector count. */
int cd_drive_extent_count(void);
int cd_drive_extent(int index, const char **path, uint32_t *first_lba, uint32_t *sectors);
/* The host path of extent `index`, for checks that compare against the file. */
const char *cd_drive_host_path(int index);

#endif
