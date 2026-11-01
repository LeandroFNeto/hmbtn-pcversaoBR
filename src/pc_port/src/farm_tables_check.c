/* The farm's animation tables of contents, checked at startup when HMBTN_SELFTEST=1.
 * Their offsets follow the count as a zero-length array's static initializer, which the
 * host keeps only through the patched anim.h (cmake/host.cmake). */

#include "farm_data.h"

#include <SDL.h>

#define SLOT_TOCS 7
#define ITEM_TOCS 10
#define PACK_TOCS 50
#define MAX_COUNT 64

static int check(int ok, const char *what) {
    SDL_Log("farm table check %s: %s", ok ? "ok" : "FAILED", what);
    return ok ? 0 : 1;
}

/* Slot and item .ANI files start at 0; a pack's start after its header of count + 1 words. */
static int check_tables(const char *kind, AnimPackToc **tocs, int n, int header) {
    AnimPackToc *toc;
    s32 first;
    int failed;
    int i;

    failed = 0;
    for (i = 0; i < n; i++) {
        toc = tocs[i];
        if (toc->count < 1 || toc->count > MAX_COUNT) {
            SDL_Log("farm table check FAILED: %s %d has count %d", kind, i, (int)toc->count);
            failed++;
            continue;
        }
        first = ANIM_PACK_START(toc, 0);
        if (first != (header ? 4 * (toc->count + 1) : 0) || ANIM_PACK_START(toc, toc->count) <= first) {
            SDL_Log("farm table check FAILED: %s %d (count %d) starts at %d, offset %d is %d", kind, i,
                    (int)toc->count, (int)first, (int)toc->count, (int)ANIM_PACK_START(toc, toc->count));
            failed++;
        }
    }
    return failed;
}

static int starts_are(AnimPackToc *toc, const s32 *expected, int n) {
    int k;

    if (toc->count != n - 1) {
        return 0;
    }
    for (k = 0; k < n; k++) {
        if (ANIM_PACK_START(toc, k) != expected[k]) {
            return 0;
        }
    }
    return 1;
}

int farm_tables_self_check(void) {
    static const s32 slot_six[] = {0, 1008, 3072, 3688, 4236, 5376};
    static const s32 pack_zero[] = {12, 1008, 2200};
    int bad;
    int failed;

    bad = check_tables("slot", farm_slot_tocs, SLOT_TOCS, 0);
    bad += check_tables("item", farm_item_tocs, ITEM_TOCS, 0);
    bad += check_tables("pack", farm_pack_tocs, PACK_TOCS, 1);
    SDL_Log("farm table check %s: %d of %d tables hold their offsets", bad == 0 ? "ok" : "FAILED",
            SLOT_TOCS + ITEM_TOCS + PACK_TOCS - bad, SLOT_TOCS + ITEM_TOCS + PACK_TOCS);
    failed = bad != 0;
    failed |= check(starts_are(farm_slot_tocs[6], slot_six, 6), "slot 6 starts 0, 1008, 3072, 3688, 4236, 5376");
    failed |= check(starts_are(farm_pack_tocs[0], pack_zero, 3), "pack 0 starts 12, 1008, 2200");
    return failed;
}
