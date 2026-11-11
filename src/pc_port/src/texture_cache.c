/* Static LRU Texture Cache for host HD asset injection with zero runtime heap allocation. */

#include <texture_cache.h>

#include <SDL.h>
#include <GL/gl.h>

#include <stdio.h>
#include <string.h>

#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif
#ifndef GL_BGR
#define GL_BGR 0x80E0
#endif

#define HASH_NIL 0xFFFF

typedef struct TextureCacheEntry {
    uint64_t key;
    GLuint gl_name;
    uint32_t last_frame;
    uint16_t prev;
    uint16_t next;
    uint8_t in_use;
} TextureCacheEntry;

static TextureCacheEntry cache_entries[TEXTURE_CACHE_CAP];
static uint16_t hash_table[TEXTURE_HASH_CAP];
static uint16_t lru_head = HASH_NIL;
static uint16_t lru_tail = HASH_NIL;
static uint16_t cache_count = 0;
static uint32_t current_frame = 0;
static TextureCacheStats cache_stats;

static TextureUploadFn custom_uploader = NULL;
static void *custom_userdata = NULL;

static inline uint32_t hash_key(uint64_t k) {
    k ^= k >> 33;
    k *= 0xff51afd7ed558ccdULL;
    k ^= k >> 33;
    k *= 0xc4ceb9fe1a85ec53ULL;
    k ^= k >> 33;
    return (uint32_t)k;
}

static GLuint default_uploader(const char *path, void *userdata) {
    SDL_Surface *image;
    GLuint name;
    GLenum format;

    (void)userdata;
    if (path == NULL || path[0] == '\0') {
        return 0;
    }
    image = SDL_LoadBMP(path);
    if (image == NULL || image->w <= 0 || image->h <= 0 || image->format->BytesPerPixel < 3) {
        if (image != NULL) {
            SDL_FreeSurface(image);
        }
        return 0;
    }
    name = 0;
    glGenTextures(1, &name);
    if (name == 0) {
        SDL_FreeSurface(image);
        return 0;
    }
    glBindTexture(GL_TEXTURE_2D, name);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    format = image->format->Rmask == 0x000000FFu ? GL_RGB : GL_BGR;
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, image->w, image->h, 0, format, GL_UNSIGNED_BYTE, image->pixels);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glBindTexture(GL_TEXTURE_2D, 0);
    SDL_FreeSurface(image);
    return name;
}

static void hash_insert(uint64_t key, uint16_t entry_idx) {
    uint32_t mask = TEXTURE_HASH_CAP - 1;
    uint32_t idx = hash_key(key) & mask;

    while (hash_table[idx] != HASH_NIL) {
        if (hash_table[idx] == entry_idx) {
            return;
        }
        idx = (idx + 1) & mask;
    }
    hash_table[idx] = entry_idx;
}

static uint16_t hash_find(uint64_t key) {
    uint32_t mask = TEXTURE_HASH_CAP - 1;
    uint32_t idx = hash_key(key) & mask;

    while (hash_table[idx] != HASH_NIL) {
        uint16_t e = hash_table[idx];
        if (e < TEXTURE_CACHE_CAP && cache_entries[e].in_use && cache_entries[e].key == key) {
            return e;
        }
        idx = (idx + 1) & mask;
    }
    return HASH_NIL;
}

static void hash_remove(uint64_t key) {
    uint32_t mask = TEXTURE_HASH_CAP - 1;
    uint32_t i = hash_key(key) & mask;

    while (hash_table[i] != HASH_NIL) {
        uint16_t e = hash_table[i];
        if (e < TEXTURE_CACHE_CAP && cache_entries[e].in_use && cache_entries[e].key == key) {
            break;
        }
        i = (i + 1) & mask;
    }
    if (hash_table[i] == HASH_NIL) {
        return;
    }
    uint32_t j = i;
    while (1) {
        j = (j + 1) & mask;
        if (hash_table[j] == HASH_NIL) {
            break;
        }
        uint16_t candidate = hash_table[j];
        if (candidate >= TEXTURE_CACHE_CAP || !cache_entries[candidate].in_use) {
            hash_table[j] = HASH_NIL;
            continue;
        }
        uint32_t k = hash_key(cache_entries[candidate].key) & mask;
        int should_move = 0;
        if (i <= j) {
            if (k <= i || k > j) {
                should_move = 1;
            }
        } else {
            if (k <= i && k > j) {
                should_move = 1;
            }
        }
        if (should_move) {
            hash_table[i] = hash_table[j];
            i = j;
        }
    }
    hash_table[i] = HASH_NIL;
}

static void lru_detach(uint16_t idx) {
    uint16_t p = cache_entries[idx].prev;
    uint16_t n = cache_entries[idx].next;

    if (p != HASH_NIL) {
        cache_entries[p].next = n;
    } else {
        lru_head = n;
    }
    if (n != HASH_NIL) {
        cache_entries[n].prev = p;
    } else {
        lru_tail = p;
    }
    cache_entries[idx].prev = HASH_NIL;
    cache_entries[idx].next = HASH_NIL;
}

static void lru_push_head(uint16_t idx) {
    cache_entries[idx].prev = HASH_NIL;
    cache_entries[idx].next = lru_head;
    if (lru_head != HASH_NIL) {
        cache_entries[lru_head].prev = idx;
    } else {
        lru_tail = idx;
    }
    lru_head = idx;
}

void texture_cache_init(void) {
    int i;

    for (i = 0; i < TEXTURE_HASH_CAP; i++) {
        hash_table[i] = HASH_NIL;
    }
    for (i = 0; i < TEXTURE_CACHE_CAP; i++) {
        cache_entries[i].in_use = 0;
        cache_entries[i].gl_name = 0;
        cache_entries[i].prev = HASH_NIL;
        cache_entries[i].next = HASH_NIL;
    }
    lru_head = HASH_NIL;
    lru_tail = HASH_NIL;
    cache_count = 0;
    memset(&cache_stats, 0, sizeof(cache_stats));
    current_frame = 0;
    memset(&cache_stats, 0, sizeof(cache_stats));
}

void texture_cache_clear(void) {
    int i;

    for (i = 0; i < TEXTURE_CACHE_CAP; i++) {
        if (cache_entries[i].in_use && cache_entries[i].gl_name != 0) {
            glDeleteTextures(1, &cache_entries[i].gl_name);
            cache_entries[i].gl_name = 0;
        }
        cache_entries[i].in_use = 0;
        cache_entries[i].prev = HASH_NIL;
        cache_entries[i].next = HASH_NIL;
    }
    for (i = 0; i < TEXTURE_HASH_CAP; i++) {
        hash_table[i] = HASH_NIL;
    }
    lru_head = HASH_NIL;
    lru_tail = HASH_NIL;
    cache_count = 0;
    memset(&cache_stats, 0, sizeof(cache_stats));
}

void texture_cache_shutdown(void) {
    texture_cache_clear();
    custom_uploader = NULL;
    custom_userdata = NULL;
}

void texture_cache_tick(uint32_t frame_num) {
    current_frame = frame_num;
}

void texture_cache_set_uploader(TextureUploadFn uploader, void *userdata) {
    custom_uploader = uploader;
    custom_userdata = userdata;
}

void texture_cache_get_stats(TextureCacheStats *stats) {
    if (stats != NULL) {
        *stats = cache_stats;
        stats->active_count = cache_count;
    }
}

GLuint texture_cache_acquire(uint64_t key, const char *path) {
    uint16_t found;
    uint16_t slot;
    GLuint name;
    TextureUploadFn uploader;

    found = hash_find(key);
    if (found != HASH_NIL) {
        /* Cache hit: move to MRU head and update last access */
        lru_detach(found);
        lru_push_head(found);
        cache_entries[found].last_frame = current_frame;
        cache_stats.hits++;
        return cache_entries[found].gl_name;
    }

    /* Cache miss */
    cache_stats.misses++;

    /* If capacity reached, evict LRU tail */
    if (cache_count >= TEXTURE_CACHE_CAP) {
        uint16_t victim = lru_tail;
        if (victim != HASH_NIL) {
            if (cache_entries[victim].gl_name != 0) {
                glDeleteTextures(1, &cache_entries[victim].gl_name);
                cache_entries[victim].gl_name = 0;
            }
            hash_remove(cache_entries[victim].key);
            lru_detach(victim);
            cache_entries[victim].in_use = 0;
            cache_count--;
            cache_stats.evictions++;
            slot = victim;
        } else {
            slot = 0;
        }
    } else {
        /* Find free slot */
        for (slot = 0; slot < TEXTURE_CACHE_CAP; slot++) {
            if (!cache_entries[slot].in_use) {
                break;
            }
        }
        if (slot >= TEXTURE_CACHE_CAP) {
            slot = 0;
        }
    }

    /* Upload texture to OpenGL */
    uploader = custom_uploader != NULL ? custom_uploader : default_uploader;
    name = uploader(path, custom_userdata);
    if (name == 0) {
        return 0;
    }

    cache_entries[slot].key = key;
    cache_entries[slot].gl_name = name;
    cache_entries[slot].last_frame = current_frame;
    cache_entries[slot].in_use = 1;

    hash_insert(key, slot);
    lru_push_head(slot);
    cache_count++;
    cache_stats.uploads++;

    return name;
}
