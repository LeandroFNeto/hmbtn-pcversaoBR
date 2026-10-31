#include <cd_drive.h>

#include <SDL.h>

#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define CD_DRIVE_MAX_EXTENTS 64
#define CD_DRIVE_PATH_MAX 512
#define XA_RAW_SECTOR 2336u

typedef struct {
    char disc_path[64];            /* upper case, '/' separators: "STR/01.XA" */
    char host_path[CD_DRIVE_PATH_MAX];
    uint32_t first_lba;
    uint32_t sectors;
    uint32_t iso_size;
    int form2;
    FILE *file;
} Extent;

static Extent extents[CD_DRIVE_MAX_EXTENTS];
static int extent_count;
static int drive_open;
static char drive_dir[CD_DRIVE_PATH_MAX];

static void upper_copy(char *dst, size_t size, const char *src) {
    size_t i;

    for (i = 0; i + 1 < size && src[i] != '\0'; i++) {
        dst[i] = (char)toupper((unsigned char)src[i]);
    }
    dst[i] = '\0';
}

static int ends_with(const char *s, const char *suffix) {
    size_t n;
    size_t m;

    n = strlen(s);
    m = strlen(suffix);
    return n >= m && strcmp(s + n - m, suffix) == 0;
}

static void walk(const char *host_dir, const char *disc_prefix) {
    DIR *dir;
    struct dirent *entry;
    struct stat st;
    char host_path[CD_DRIVE_PATH_MAX];
    char disc_path[64];
    Extent *e;

    dir = opendir(host_dir);
    if (dir == NULL) {
        return;
    }
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        snprintf(host_path, sizeof(host_path), "%s/%s", host_dir, entry->d_name);
        snprintf(disc_path, sizeof(disc_path), "%s%s", disc_prefix, entry->d_name);
        if (stat(host_path, &st) != 0) {
            continue;
        }
        if (S_ISDIR(st.st_mode)) {
            char prefix[64];

            snprintf(prefix, sizeof(prefix), "%s/", disc_path);
            walk(host_path, prefix);
            continue;
        }
        if (strcmp(entry->d_name, "license_data.dat") == 0 || extent_count >= CD_DRIVE_MAX_EXTENTS) {
            continue;
        }
        e = &extents[extent_count++];
        memset(e, 0, sizeof(*e));
        upper_copy(e->disc_path, sizeof(e->disc_path), disc_path);
        snprintf(e->host_path, sizeof(e->host_path), "%s", host_path);
        e->form2 = ends_with(e->disc_path, ".XA");
        if (e->form2) {
            e->sectors = (uint32_t)st.st_size / XA_RAW_SECTOR;
            e->iso_size = e->sectors * CD_DRIVE_SECTOR;
        } else {
            e->sectors = ((uint32_t)st.st_size + CD_DRIVE_SECTOR - 1) / CD_DRIVE_SECTOR;
            e->iso_size = (uint32_t)st.st_size;
        }
    }
    closedir(dir);
}

static int compare_extents(const void *a, const void *b) {
    return strcmp(((const Extent *)a)->disc_path, ((const Extent *)b)->disc_path);
}

int cd_drive_open(const char *dir) {
    const char *env;
    char *base;
    uint32_t lba;
    int i;

    if (drive_open) {
        return 0;
    }
    if (dir == NULL) {
        env = getenv("HMBTN_DISC_DIR");
        if (env != NULL && env[0] != '\0') {
            snprintf(drive_dir, sizeof(drive_dir), "%s", env);
        } else {
            base = SDL_GetBasePath();
            snprintf(drive_dir, sizeof(drive_dir), "%sdisc", base != NULL ? base : "");
            SDL_free(base);
        }
    } else {
        snprintf(drive_dir, sizeof(drive_dir), "%s", dir);
    }

    extent_count = 0;
    walk(drive_dir, "");
    if (extent_count == 0) {
        SDL_Log("CD drive: no files in %s", drive_dir);
        return -1;
    }
    qsort(extents, (size_t)extent_count, sizeof(extents[0]), compare_extents);
    lba = CD_DRIVE_FIRST_LBA;
    for (i = 0; i < extent_count; i++) {
        extents[i].first_lba = lba;
        lba += extents[i].sectors;
    }
    drive_open = 1;
    SDL_Log("CD drive: %d files from %s", extent_count, drive_dir);
    return 0;
}

int cd_drive_is_open(void) {
    return drive_open;
}

const char *cd_drive_dir(void) {
    return drive_dir;
}

/* "\STR\01.XA;1" -> "STR/01.XA" */
static void normalize(char *dst, size_t size, const char *name) {
    size_t i;

    while (*name == '\\' || *name == '/') {
        name++;
    }
    for (i = 0; i + 1 < size && name[i] != '\0' && name[i] != ';'; i++) {
        dst[i] = name[i] == '\\' ? '/' : (char)toupper((unsigned char)name[i]);
    }
    dst[i] = '\0';
}

int cd_drive_find(const char *disc_name, uint32_t *first_lba, uint32_t *size) {
    char key[64];
    int i;

    if (!drive_open || disc_name == NULL) {
        return -1;
    }
    normalize(key, sizeof(key), disc_name);
    for (i = 0; i < extent_count; i++) {
        if (strcmp(extents[i].disc_path, key) == 0) {
            *first_lba = extents[i].first_lba;
            *size = extents[i].iso_size;
            return 0;
        }
    }
    return -1;
}

static Extent *extent_at(uint32_t lba) {
    int lo;
    int hi;
    int mid;

    lo = 0;
    hi = extent_count - 1;
    while (lo <= hi) {
        mid = (lo + hi) / 2;
        if (lba < extents[mid].first_lba) {
            hi = mid - 1;
        } else if (lba >= extents[mid].first_lba + extents[mid].sectors) {
            lo = mid + 1;
        } else {
            return &extents[mid];
        }
    }
    return NULL;
}

int cd_drive_read(uint32_t lba, uint32_t sectors, void *dst) {
    unsigned char *out;
    Extent *e;
    size_t got;
    uint32_t n;
    int status;

    out = (unsigned char *)dst;
    status = 0;
    for (n = 0; n < sectors; n++, lba++, out += CD_DRIVE_SECTOR) {
        memset(out, 0, CD_DRIVE_SECTOR);
        e = extent_at(lba);
        if (e == NULL || e->form2) {
            continue;
        }
        if (e->file == NULL) {
            e->file = fopen(e->host_path, "rb");
            if (e->file == NULL) {
                SDL_Log("CD drive: cannot open %s", e->host_path);
                status = -1;
                continue;
            }
        }
        if (fseek(e->file, (long)((lba - e->first_lba) * CD_DRIVE_SECTOR), SEEK_SET) != 0) {
            status = -1;
            continue;
        }
        got = fread(out, 1, CD_DRIVE_SECTOR, e->file);
        if (got < CD_DRIVE_SECTOR && ferror(e->file)) {
            clearerr(e->file);
            status = -1;
        }
    }
    return status;
}

int cd_drive_extent_count(void) {
    return extent_count;
}

int cd_drive_extent(int index, const char **path, uint32_t *first_lba, uint32_t *sectors) {
    if (index < 0 || index >= extent_count) {
        return -1;
    }
    *path = extents[index].disc_path;
    *first_lba = extents[index].first_lba;
    *sectors = extents[index].sectors;
    return 0;
}

const char *cd_drive_host_path(int index) {
    if (index < 0 || index >= extent_count) {
        return NULL;
    }
    return extents[index].host_path;
}
