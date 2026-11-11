/* BMP files whose names are a page and UV bounds. The game files are not edited. */

#include <mods.h>
#include <texture_cache.h>

#include <SDL.h>

#include <dirent.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define MOD_MAX 32

typedef struct ModSlot {
    int page_x;
    int page_y;
    int u0;
    int v0;
    int u1;
    int v1;
    char name[48];
    uint8_t duplicate;
} ModSlot;

typedef struct ModelSlot {
    unsigned addr;
    char name[48];
    uint8_t duplicate;
} ModelSlot;

static ModSlot slots[MOD_MAX];
static int slot_count;
static ModelSlot models[MOD_MAX];
static int model_count;
static char cached_folder[260];
static int ready;
static int closed;

static int same_key(const ModSlot *a, int page_x, int page_y, int u0, int v0, int u1, int v1) {
    return a->page_x == page_x && a->page_y == page_y && a->u0 == u0 && a->v0 == v0 && a->u1 == u1 && a->v1 == v1;
}

/* `pageX_pageY_u0_v0_u1_v1.bmp`, or the same six numbers after a prefix and '_'. */
static int parse_name(const char *name, int *page_x, int *page_y, int *u0, int *v0, int *u1, int *v1) {
    const char *p;

    p = name;
    while (p != NULL && *p != '\0') {
        int n;

        n = 0;
        if (sscanf(p, "%d_%d_%d_%d_%d_%d.bmp%n", page_x, page_y, u0, v0, u1, v1, &n) == 6 && p[n] == '\0') {
            return 1;
        }
        p = strchr(p, '_');
        if (p != NULL) {
            p++;
        }
    }
    return 0;
}

/* `tmd_` plus eight lowercase hex digits, optionally after a prefix. */
static int parse_model(const char *name, unsigned *addr) {
    const char *p;

    p = name;
    while (p != NULL && (p = strstr(p, "tmd_")) != NULL) {
        unsigned value;
        int i;

        value = 0;
        for (i = 0; i < 8; i++) {
            char c;

            c = p[4 + i];
            if (c >= '0' && c <= '9') {
                value = (value << 4) | (unsigned)(c - '0');
            } else if (c >= 'a' && c <= 'f') {
                value = (value << 4) | (unsigned)(c - 'a' + 10);
            } else {
                break;
            }
        }
        if (i == 8 && strcmp(p + 12, ".bmp") == 0) {
            *addr = value;
            return 1;
        }
        p += 4;
    }
    return 0;
}

static int mods_folder(char *out, int cap) {
    const char *env;
    char *base;

    env = getenv("HMBTN_MODS");
    if (env != NULL && env[0] != '\0') {
        snprintf(out, (size_t)cap, "%s", env);
        return 0;
    }
    base = SDL_GetBasePath();
    if (base == NULL) {
        return -1;
    }
    snprintf(out, (size_t)cap, "%smods/textures", base);
    SDL_free(base);
    return 0;
}

static void drop_model_duplicates(void) {
    int i;
    int j;

    for (i = 0; i < model_count; i++) {
        for (j = i + 1; j < model_count; j++) {
            if (models[j].addr != models[i].addr) {
                continue;
            }
            models[i].duplicate = 1;
            models[j].duplicate = 1;
        }
    }
}

static void drop_duplicates(void) {
    int i;
    int j;

    for (i = 0; i < slot_count; i++) {
        for (j = i + 1; j < slot_count; j++) {
            if (same_key(&slots[j], slots[i].page_x, slots[i].page_y, slots[i].u0, slots[i].v0, slots[i].u1,
                         slots[i].v1)) {
                slots[i].duplicate = 1;
                slots[j].duplicate = 1;
            }
        }
    }
}

void mods_rescan(void) {
    DIR *dir;
    struct dirent *entry;

    if (closed) {
        return;
    }
    texture_cache_clear();
    slot_count = 0;
    model_count = 0;
    ready = 1;
    if (mods_folder(cached_folder, (int)sizeof cached_folder) != 0) {
        return;
    }
    dir = opendir(cached_folder);
    if (dir == NULL) {
        return;
    }
    while ((entry = readdir(dir)) != NULL && slot_count + model_count < MOD_MAX) {
        int page_x;
        int page_y;
        int u0;
        int v0;
        int u1;
        int v1;
        unsigned addr;

        if (parse_model(entry->d_name, &addr)) {
            models[model_count].addr = addr;
            models[model_count].duplicate = 0;
            snprintf(models[model_count].name, sizeof(models[model_count].name), "%s", entry->d_name);
            model_count++;
            continue;
        }
        if (!parse_name(entry->d_name, &page_x, &page_y, &u0, &v0, &u1, &v1)) {
            continue;
        }
        slots[slot_count].page_x = page_x;
        slots[slot_count].page_y = page_y;
        slots[slot_count].u0 = u0;
        slots[slot_count].v0 = v0;
        slots[slot_count].u1 = u1;
        slots[slot_count].v1 = v1;
        slots[slot_count].duplicate = 0;
        snprintf(slots[slot_count].name, sizeof(slots[slot_count].name), "%s", entry->d_name);
        slot_count++;
    }
    closedir(dir);
    drop_duplicates();
    drop_model_duplicates();
}

void mods_shutdown(void) {
    texture_cache_shutdown();
    slot_count = 0;
    model_count = 0;
    ready = 1;
    closed = 1;
}

unsigned mods_lookup(int page_x, int page_y, int u0, int v0, int u1, int v1) {
    int i;
    int match_idx;
    uint64_t key;
    char path[512];

    if (closed) {
        return 0;
    }
    if (!ready) {
        mods_rescan();
    }
    match_idx = -1;
    for (i = 0; i < slot_count; i++) {
        if (!same_key(&slots[i], page_x, page_y, u0, v0, u1, v1)) {
            continue;
        }
        if (slots[i].duplicate) {
            return 0;
        }
        if (match_idx != -1) {
            return 0;
        }
        match_idx = i;
    }
    if (match_idx < 0) {
        return 0;
    }

    key = (((uint64_t)(uint32_t)page_x & 0xFFFFULL) << 48) |
          (((uint64_t)(uint32_t)page_y & 0xFFFFULL) << 32) |
          (((uint64_t)(uint8_t)u0) << 24) |
          (((uint64_t)(uint8_t)v0) << 16) |
          (((uint64_t)(uint8_t)u1) << 8)  |
          ((uint64_t)(uint8_t)v1);

    snprintf(path, sizeof(path), "%s/%s", cached_folder, slots[match_idx].name);
    return (unsigned)texture_cache_acquire(key, path);
}

unsigned mods_lookup_model(const void *tmd) {
    unsigned addr;
    int i;
    int match_idx;
    uint64_t key;
    char path[512];

    if (tmd == NULL || closed) {
        return 0;
    }
    if (!ready) {
        mods_rescan();
    }
    addr = (unsigned)(uintptr_t)tmd;
    match_idx = -1;
    for (i = 0; i < model_count; i++) {
        if (models[i].addr != addr) {
            continue;
        }
        if (models[i].duplicate) {
            return 0;
        }
        if (match_idx != -1) {
            return 0;
        }
        match_idx = i;
    }
    if (match_idx < 0) {
        return 0;
    }

    key = 0x8000000000000000ULL | (uint64_t)addr;
    snprintf(path, sizeof(path), "%s/%s", cached_folder, models[match_idx].name);
    return (unsigned)texture_cache_acquire(key, path);
}
