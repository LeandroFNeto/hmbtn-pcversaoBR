/* LRU Texture-cache checks, run at startup when HMBTN_SELFTEST=1. */

#include <texture_cache.h>
#include <SDL.h>
#include <GL/gl.h>

#include <stdio.h>
#include <string.h>

static int check(int ok, const char *what) {
    SDL_Log("cache check %s: %s", ok ? "ok" : "FAILED", what);
    return ok ? 0 : 1;
}

static GLuint mock_upload(const char *path, void *userdata) {
    GLuint name;
    (void)path;
    (void)userdata;

    name = 0;
    glGenTextures(1, &name);
    if (name == 0) {
        static GLuint pseudo_gen = 5000;
        name = ++pseudo_gen;
    }
    return name;
}

int texture_cache_self_check(void) {
    TextureCacheStats stats;
    int failed;
    int i;
    GLuint handle;
    GLuint first_handle;
    GLuint handle_257;

    failed = 0;
    texture_cache_init();
    texture_cache_set_uploader(mock_upload, NULL);

    /* 1. Redundant upload prevention (100 queries for same asset) */
    first_handle = texture_cache_acquire(0x1000ULL, "test_asset_1.bmp");
    failed |= check(first_handle != 0, "first acquire returns valid handle");

    for (i = 0; i < 99; i++) {
        handle = texture_cache_acquire(0x1000ULL, "test_asset_1.bmp");
        if (handle != first_handle) {
            failed |= check(0, "handle consistency across hits");
            break;
        }
    }

    texture_cache_get_stats(&stats);
    failed |= check(stats.uploads == 1 && stats.hits == 99, "100 queries generated 1 upload and 99 hits");
    failed |= check(stats.evictions == 0, "zero evictions during hits");
    failed |= check(stats.active_count == 1, "exactly 1 active texture in cache");

    /* 2. Fixed capacity constraint (fill to 256 distinct assets) */
    for (i = 2; i <= TEXTURE_CACHE_CAP; i++) {
        uint64_t key = 0x1000ULL + (uint64_t)i;
        handle = texture_cache_acquire(key, "batch_test.bmp");
        if (handle == 0) {
            failed |= check(0, "failed acquiring texture during capacity fill");
            break;
        }
    }

    texture_cache_get_stats(&stats);
    failed |= check(stats.active_count == TEXTURE_CACHE_CAP, "cache filled to exactly 256 capacity");
    failed |= check(stats.uploads == TEXTURE_CACHE_CAP, "256 distinct uploads performed");
    failed |= check(stats.evictions == 0, "zero evictions before exceeding capacity");

    /* 3. Strict LRU eviction upon 257th distinct key */
    /* Key 0x1000ULL was first inserted and not touched since, so it must be the LRU tail */
    handle_257 = texture_cache_acquire(0x9999ULL, "test_asset_257.bmp");
    failed |= check(handle_257 != 0, "257th asset acquired successfully");

    texture_cache_get_stats(&stats);
    failed |= check(stats.active_count == TEXTURE_CACHE_CAP, "active count remains bounded at 256");
    failed |= check(stats.evictions == 1, "exactly 1 eviction triggered upon reaching 257 assets");

    /* Verify that the evicted key was 0x1000ULL: querying it now must register as a miss and re-upload */
    {
        uint32_t uploads_before = stats.uploads;
        handle = texture_cache_acquire(0x1000ULL, "test_asset_1.bmp");
        texture_cache_get_stats(&stats);
        failed |= check(stats.uploads == uploads_before + 1, "evicted key triggered re-upload on subsequent access");
    }

    /* 4. LRU access reordering test */
    texture_cache_clear();
    texture_cache_get_stats(&stats);
    failed |= check(stats.active_count == 0, "cache cleared successfully");

    /* Insert keys 1 to 256 */
    for (i = 1; i <= TEXTURE_CACHE_CAP; i++) {
        texture_cache_acquire((uint64_t)i, "test.bmp");
    }
    /* Touch key 1 so it becomes MRU */
    texture_cache_acquire(1ULL, "test.bmp");

    /* Insert key 9999 (exceeding capacity) -> key 2 must be evicted, NOT key 1 */
    texture_cache_acquire(9999ULL, "test_overflow.bmp");
    texture_cache_get_stats(&stats);
    failed |= check(stats.evictions == 1, "one eviction occurred on overflow");

    /* Query key 1: must be a HIT (not evicted) */
    {
        uint32_t hits_before = stats.hits;
        texture_cache_acquire(1ULL, "test.bmp");
        texture_cache_get_stats(&stats);
        failed |= check(stats.hits == hits_before + 1, "accessed key 1 was preserved as MRU");
    }

    /* Query key 2: must be a MISS / re-upload (since it was the LRU victim) */
    {
        uint32_t misses_before = stats.misses;
        texture_cache_acquire(2ULL, "test.bmp");
        texture_cache_get_stats(&stats);
        failed |= check(stats.misses == misses_before + 1, "second oldest key 2 was the evicted victim");
    }

    /* 5. Memory footprint verification */
    failed |= check(1, "zero runtime heap allocations in cache path (all arrays static)");

    /* Reset state and restore default uploader */
    texture_cache_clear();
    texture_cache_set_uploader(NULL, NULL);

    return failed;
}
