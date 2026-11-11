#ifndef TEXTURE_CACHE_H
#define TEXTURE_CACHE_H

#include <stdint.h>
#include <GL/gl.h>

#define TEXTURE_CACHE_CAP 256
#define TEXTURE_HASH_CAP  512

typedef struct TextureCacheStats {
    uint32_t hits;
    uint32_t misses;
    uint32_t uploads;
    uint32_t evictions;
    uint32_t active_count;
} TextureCacheStats;

typedef GLuint (*TextureUploadFn)(const char *path, void *userdata);

void texture_cache_init(void);
void texture_cache_shutdown(void);
void texture_cache_clear(void);
void texture_cache_tick(uint32_t frame_num);

/* Returns OpenGL texture handle for key. If not cached, invokes uploader and inserts into cache.
 * Evicts LRU entry via glDeleteTextures when capacity is reached. */
GLuint texture_cache_acquire(uint64_t key, const char *path);

/* Allows overriding the uploader routine (used by automated tests) */
void texture_cache_set_uploader(TextureUploadFn uploader, void *userdata);

/* Query runtime statistics */
void texture_cache_get_stats(TextureCacheStats *stats);

/* Selftest hook for HMBTN_SELFTEST=1 */
int texture_cache_self_check(void);

#endif /* TEXTURE_CACHE_H */
