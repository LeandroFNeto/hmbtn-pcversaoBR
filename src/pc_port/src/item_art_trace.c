/* Why an in-memory item-art lookup returned null. The game file is not edited:
 * picture_cells.c's host copy calls item_art_note. */

#include <item_art_trace.h>

#include "anim.h"
#include "farm_data.h"
#include "farm_state.h"
#include "scene.h"

#include <SDL.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define ITEM_ART_BASKET 0x064
#define ITEM_ART_SEEN 1024
#define ITEM_ART_LINE 160

static unsigned char seen[ITEM_ART_SEEN];
static int notes;

static int watched(int id, int scene, int has_mode) {
    if (id == ITEM_ART_BASKET) {
        return 1;
    }
    return has_mode && scene >= AREA_HOUSE0_SPRING && scene <= AREA_HOUSE2_WINTER;
}

/* The lookup's exits, in the order item_art_address tests them. `head` null is no mode.
 * A resource set outside 0..9 does not index the list or the table. */
static void format_line(const FarmZoneHeader *head, int set, const s16 *list, const AnimPackToc *toc, int id,
                        char *out, int cap) {
    int list_count;
    int count;

    if (head == NULL) {
        snprintf(out, cap, "itemart: id 0x%03x exit no-mode", id);
        return;
    }
    if (set == -1) {
        snprintf(out, cap, "itemart: id 0x%03x exit no-set", id);
        return;
    }
    if (set < 0 || set > 9) {
        snprintf(out, cap, "itemart: id 0x%03x exit set-range %d", id, set);
        return;
    }
    if (list == NULL) {
        snprintf(out, cap, "itemart: id 0x%03x exit null-list", id);
        return;
    }
    list_count = list[0];
    if (toc == NULL) {
        snprintf(out, cap, "itemart: id 0x%03x exit absent list_count %d toc 0x00000000 count unread start unread", id,
                 list_count);
        return;
    }
    count = (int)toc->count;
    if (count < 1 || count > 63) {
        snprintf(out, cap, "itemart: id 0x%03x exit absent list_count %d toc 0x%08x count %d start unread", id,
                 list_count, (unsigned)(uintptr_t)toc, count);
        return;
    }
    snprintf(out, cap, "itemart: id 0x%03x exit absent list_count %d toc 0x%08x count %d start %d", id, list_count,
             (unsigned)(uintptr_t)toc, count, (int)ANIM_PACK_START(toc, 0));
}

void *item_art_note(int id, void *found) {
    FarmModeScript *mode;
    const s16 *list;
    const AnimPackToc *toc;
    int set;
    int scene;
    int has_mode;
    char line[ITEM_ART_LINE];

    if (found != NULL || id < 0 || id >= ITEM_ART_SEEN || seen[id]) {
        return found;
    }
    mode = farm_area.mode.data;
    has_mode = mode != NULL;
    scene = has_mode ? (int)mode->head.scene : 0;
    if (!watched(id, scene, has_mode)) {
        return found;
    }
    set = has_mode ? (int)mode->head.resourceSet : 0;
    list = NULL;
    toc = NULL;
    if (has_mode && set >= 0 && set <= 9) {
        list = resource_lists[set];
        toc = farm_item_tocs[set];
    }
    seen[id] = 1;
    format_line(has_mode ? &mode->head : NULL, set, list, toc, id, line, (int)sizeof line);
    SDL_Log("%s", line);
    notes++;
    return found;
}

/* picture_load_to_vram is the game's load. The host copy notes a null lookup on the way. */
s32 picture_load_to_vram(s32 sprite);

static int place_wants_basket(int place) {
    return place == PLACE_FARM;
}

void item_art_keep_for_place(int place) {
    if (place_wants_basket(place)) {
        picture_load_to_vram(ITEM_ART_BASKET);
    }
}

static int check(int ok, const char *what) {
    SDL_Log("itemart check %s: %s", ok ? "ok" : "FAILED", what);
    return ok ? 0 : 1;
}

int item_art_self_check(void) {
    static s16 list[3] = {2, 1, 2};
    static s32 toc_words[2] = {35, 0};
    static s32 zero_words[1] = {0};
    FarmZoneHeader head;
    AnimPackToc *toc;
    AnimPackToc *zero;
    char line[ITEM_ART_LINE];
    char expect[32];
    int failed;
    int before;
    int once;
    int marker;

    memset(&head, 0, sizeof head);
    toc = (AnimPackToc *)toc_words;
    zero = (AnimPackToc *)zero_words;
    failed = 0;

    format_line(&head, 0, list, toc, ITEM_ART_BASKET, line, (int)sizeof line);
    snprintf(expect, sizeof expect, "toc 0x%08x", (unsigned)(uintptr_t)toc);
    failed |= check(strstr(line, "exit absent") != NULL && strstr(line, "list_count 2") != NULL &&
                        strstr(line, expect) != NULL && strstr(line, "count 35") != NULL &&
                        strstr(line, "start 0") != NULL,
                    line);

    format_line(&head, 12, NULL, NULL, ITEM_ART_BASKET, line, (int)sizeof line);
    failed |= check(strstr(line, "exit set-range 12") != NULL && strstr(line, "toc") == NULL, line);

    format_line(&head, 0, list, zero, ITEM_ART_BASKET, line, (int)sizeof line);
    failed |= check(strstr(line, "count 0") != NULL && strstr(line, "start unread") != NULL, line);

    failed |= check(watched(0x10, AREA_HOUSE0_SPRING, 1) && watched(0x10, AREA_HOUSE2_WINTER, 1) &&
                        !watched(0x10, AREA_HOUSE0_SPRING - 1, 1) && !watched(0x10, AREA_HOUSE2_WINTER + 1, 1),
                    "house scenes 0x50 through 0x5B");

    failed |= check(place_wants_basket(PLACE_FARM) && !place_wants_basket(PLACE_GREENHOUSE) &&
                        !place_wants_basket(0),
                    "shipping bin load follows the farm map");

    before = notes;
    marker = 1;
    item_art_note(ITEM_ART_BASKET, &marker);
    failed |= check(notes == before, "non-null address logged nothing");
    item_art_note(ITEM_ART_BASKET, NULL);
    once = notes;
    item_art_note(ITEM_ART_BASKET, NULL);
    failed |= check(once == before + 1 && notes == once, "a second miss of 0x064 added no line");
    seen[ITEM_ART_BASKET] = 0;

    return failed;
}
