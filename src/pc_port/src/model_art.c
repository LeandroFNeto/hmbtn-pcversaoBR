/* Look rules: one object and one character. A row with no model or no UV
 * matches nothing, so the game polygons stay on the sample they calculated. */

#include <host.h>
#include <model_art.h>
#include <model_tex.h>
#include <mods.h>
#include <vram.h>

#include "farm_state.h"
#include "main.h"
#include "model.h"

#include <SDL.h>

#include <direct.h>
#include <stdio.h>
#include <string.h>

#define MODEL_ART_CLUT_RESERVED 1008
#define MODEL_ART_RULES 2
#define MODEL_ART_TEST_MAX 2

typedef struct LookRule {
    const void *model;
    short places[3];
    int nplaces;
    int page_x;
    int page_y;
    int u0;
    int v0;
    int u1;
    int v1;
    int uv_set;
    int picture;
    int live;
    int cell_page_x;
    int cell_page_y;
    int depth;
    int clut_x;
    int clut_y;
    int cell_u0;
    int cell_v0;
    int cell_u1;
    int cell_v1;
    short noted;
} LookRule;

static LookRule rules[MODEL_ART_RULES];
static int rules_ready;
static int test_on;
static LookRule test_rules[MODEL_ART_TEST_MAX];
static int test_n;

static int picture_present(int id) {
    int row;
    int col;

    for (row = 0; row < PICTURE_ROWS; row++) {
        for (col = 0; col < PICTURE_SLOTS; col++) {
            if (farm_area.mode.pictureIds[row][col] == (s16)id) {
                return 1;
            }
        }
    }
    return 0;
}

static int cell_page_x(int clut_x) {
    return clut_x >= MODEL_ART_CLUT_RESERVED ? 960 : 832;
}

static int words_nonzero(int page_x, int page_y, int u0, int v0, int u1, int v1) {
    int u;
    int v;
    int x;
    int y;

    if (u1 < u0) {
        u = u0;
        u0 = u1;
        u1 = u;
    }
    if (v1 < v0) {
        v = v0;
        v0 = v1;
        v1 = v;
    }
    if (u1 == u0) {
        u1 = u0 + 1;
    }
    if (v1 == v0) {
        v1 = v0 + 1;
    }
    for (v = v0; v < v1; v++) {
        for (u = u0; u < u1; u += 4) {
            x = page_x + u / 4;
            y = page_y + v;
            if (vram[y & 511][x & 1023] != 0) {
                return 1;
            }
        }
    }
    return 0;
}

static void cell_from_grid(LookRule *rule) {
    int row;
    int col;
    FarmGridEntry *entry;
    int u0;
    int v0;
    int u1;
    int v1;

    rule->live = 0;
    for (row = 0; row < PICTURE_ROWS; row++) {
        for (col = 0; col < PICTURE_SLOTS; col++) {
            if (farm_area.mode.pictureIds[row][col] != (s16)rule->picture) {
                continue;
            }
            entry = &farm_area.mode.pictureEntries[row][col];
            rule->cell_page_x = cell_page_x(entry->clutX);
            rule->cell_page_y = 0;
            rule->depth = 4;
            rule->clut_x = entry->clutX;
            rule->clut_y = entry->clutY;
            u0 = entry->x * 4;
            v0 = entry->y;
            u1 = u0;
            v1 = v0;
            if (entry->w > 0 && entry->w <= 256) {
                u1 = u0 + entry->w;
            }
            if (entry->h > 0 && entry->h <= 256) {
                v1 = v0 + entry->h;
            }
            rule->cell_u0 = u0;
            rule->cell_v0 = v0;
            rule->cell_u1 = u1;
            rule->cell_v1 = v1;
            rule->live = words_nonzero(rule->cell_page_x, rule->cell_page_y, u0, v0, u1, v1);
            return;
        }
    }
}

static int place_named(const LookRule *rule, short place) {
    int i;

    for (i = 0; i < rule->nplaces; i++) {
        if (rule->places[i] == place) {
            return 1;
        }
    }
    return 0;
}

static void prepare_rules(void) {
    if (rules_ready) {
        return;
    }
    memset(rules, 0, sizeof rules);
    rules[0].picture = 0x064;
    rules[0].places[0] = PLACE_FARM;
    rules[0].places[1] = PLACE_4;
    rules[0].places[2] = PLACE_5;
    rules[0].nplaces = 3;
    rules[0].noted = -2;
    rules[1].picture = 0;
    rules[1].noted = -2;
    rules_ready = 1;
}

static void note_rule(LookRule *rule, short place) {
    int called;

    if (rule->picture == 0) {
        return;
    }
    if (!place_named(rule, place)) {
        if (rule->noted != place) {
            rule->noted = place;
            rule->live = 0;
        }
        return;
    }
    if (rule->noted == place) {
        return;
    }
    if (game_state.itemInfo == 0 || game_state.system.workSectorBusy != 0) {
        return;
    }
    rule->noted = place;
    rule->live = 0;
    if (picture_present(rule->picture)) {
        called = 1;
    } else {
        called = picture_load_to_vram(rule->picture);
    }
    if (called != 0) {
        cell_from_grid(rule);
    }
    SDL_Log("model art: picture %d place %d call %d live %d", rule->picture, (int)place, called, rule->live);
}

void model_art_note_place(void) {
    short place;
    int i;

    prepare_rules();
    place = farm_area.mode.place.location;
    for (i = 0; i < MODEL_ART_RULES; i++) {
        note_rule(&rules[i], place);
    }
}

static int rule_hit(const LookRule *rule, const void *model, short place, int page_x, int page_y, int u0, int v0, int u1,
                    int v1) {
    if (rule->model == NULL || !rule->uv_set || !rule->live || model == NULL || model != rule->model) {
        return 0;
    }
    if (!place_named(rule, place)) {
        return 0;
    }
    return page_x == rule->page_x && page_y == rule->page_y && u0 == rule->u0 && v0 == rule->v0 && u1 == rule->u1 &&
           v1 == rule->v1;
}

int model_art_redirect(const void *packet, int page_x, int page_y, int u0, int v0, int u1, int v1, int replaced,
                       int *out_page_x, int *out_page_y, int *out_depth, int *out_clut_x, int *out_clut_y, int *out_u0,
                       int *out_v0, int *out_u1, int *out_v1) {
    const void *model;
    const LookRule *table;
    int n;
    int i;
    short place;

    if (replaced) {
        return 0;
    }
    prepare_rules();
    model = model_tex_lookup(packet);
    place = farm_area.mode.place.location;
    table = test_on ? test_rules : rules;
    n = test_on ? test_n : MODEL_ART_RULES;
    for (i = 0; i < n; i++) {
        if (!rule_hit(&table[i], model, place, page_x, page_y, u0, v0, u1, v1)) {
            continue;
        }
        *out_page_x = table[i].cell_page_x;
        *out_page_y = table[i].cell_page_y;
        *out_depth = table[i].depth;
        *out_clut_x = table[i].clut_x;
        *out_clut_y = table[i].clut_y;
        *out_u0 = table[i].cell_u0;
        *out_v0 = table[i].cell_v0;
        *out_u1 = table[i].cell_u1;
        *out_v1 = table[i].cell_v1;
        return 1;
    }
    return 0;
}

void model_art_test_begin(void) {
    test_on = 1;
    test_n = 0;
    memset(test_rules, 0, sizeof test_rules);
}

void model_art_test_add(const void *model, const short *places, int nplaces, int page_x, int page_y, int u0, int v0,
                        int u1, int v1, int cell_page_x, int cell_page_y, int depth, int clut_x, int clut_y, int cell_u0,
                        int cell_v0, int cell_u1, int cell_v1, int live) {
    LookRule *rule;
    int i;

    if (test_n >= MODEL_ART_TEST_MAX) {
        return;
    }
    rule = &test_rules[test_n++];
    rule->model = model;
    rule->nplaces = nplaces > 3 ? 3 : nplaces;
    for (i = 0; i < rule->nplaces; i++) {
        rule->places[i] = places[i];
    }
    rule->page_x = page_x;
    rule->page_y = page_y;
    rule->u0 = u0;
    rule->v0 = v0;
    rule->u1 = u1;
    rule->v1 = v1;
    rule->uv_set = 1;
    rule->live = live;
    rule->cell_page_x = cell_page_x;
    rule->cell_page_y = cell_page_y;
    rule->depth = depth;
    rule->clut_x = clut_x;
    rule->clut_y = clut_y;
    rule->cell_u0 = cell_u0;
    rule->cell_v0 = cell_v0;
    rule->cell_u1 = cell_u1;
    rule->cell_v1 = cell_v1;
}

void model_art_test_end(void) {
    test_on = 0;
    test_n = 0;
}

static POLY_FT4 art_quad;
static TmdHeader art_object;
static TmdHeader art_character;
static TmdHeader art_other;
static char art_env[640];

static int art_check(int ok, const char *what) {
    SDL_Log("model art check %s: %s", ok ? "ok" : "FAILED", what);
    return ok ? 0 : 1;
}

static int art_near(int value, int expected) {
    int d;

    d = value - expected;
    if (d < 0) {
        d = -d;
    }
    return d <= 8;
}

static int art_read(int x, int y, int r, int g, int b) {
    unsigned char rgb[3];

    gpu_debug_read_pixel(x, y, rgb);
    SDL_Log("model art check pixel %d,%d,%d", rgb[0], rgb[1], rgb[2]);
    return art_near(rgb[0], r) && art_near(rgb[1], g) && art_near(rgb[2], b);
}

static void own_packet(void *packet, int bytes, const void *model) {
    FrameState *saved;
    static FrameState frame;

    saved = current_frame;
    memset(&frame, 0, sizeof frame);
    current_frame = &frame;
    frame.primCursor.bytes = packet;
    model_tex_begin(model);
    model_tex_hook(0x2000);
    frame.primCursor.bytes = (unsigned char *)packet + bytes;
    model_tex_end();
    current_frame = saved;
}

static void draw_owned(const void *model, int x, int y, int page_x, int u0, int v0, int u1, int v1, int r, int g, int b) {
    int tp;

    tp = getTPage(0, 0, page_x, 0);
    memset(&art_quad, 0, sizeof art_quad);
    setPolyFT4(&art_quad);
    setSemiTrans(&art_quad, 0);
    setShadeTex(&art_quad, 1);
    setRGB0(&art_quad, r, g, b);
    setXY4(&art_quad, x, y, x + 8, y, x, y + 8, x + 8, y + 8);
    setUV4(&art_quad, u0, v0, u1, v0, u0, v1, u1, v1);
    art_quad.clut = getClut(0, 480);
    art_quad.tpage = (u_short)tp;
    termPrim(&art_quad);
    own_packet(&art_quad, (int)sizeof art_quad, model);
    DrawOTag((u_long *)&art_quad);
}

static void arm_pair(int object_live, int object_u, uint16_t object_word, uint16_t character_word) {
    static const short object_places[] = {PLACE_FARM, PLACE_4, PLACE_5};
    static const short character_places[] = {PLACE_FARM};

    vram_load(620, 430, 1, 1, &object_word);
    vram_load(621, 430, 1, 1, &character_word);
    model_art_test_begin();
    model_art_test_add(&art_object, object_places, 3, 192, 0, 8, 8, 16, 16, 620, 430, 15, 0, 0, object_u, 0, object_u, 0,
                       object_live);
    model_art_test_add(&art_character, character_places, 1, 192, 0, 8, 8, 16, 16, 620, 430, 15, 0, 0, 1, 0, 1, 0, 1);
}

static void write_red(const char *path) {
    SDL_Surface *image;

    image = SDL_CreateRGBSurface(0, 8, 8, 24, 0x000000FFu, 0x0000FF00u, 0x00FF0000u, 0);
    if (image == NULL) {
        return;
    }
    SDL_FillRect(image, NULL, SDL_MapRGB(image->format, 255, 0, 0));
    SDL_SaveBMP(image, path);
    SDL_FreeSurface(image);
}

int model_art_self_check(void) {
    uint16_t red;
    uint16_t green;
    uint16_t zero;
    short saved;
    char *base;
    char dir[512];
    char path[640];
    int failed;

    failed = 0;
    red = 0x001F;
    green = 0x03E0;
    zero = 0;
    saved = farm_area.mode.place.location;
    vram_fill(194, 8, 4, 10, zero);
    vram_fill(202, 40, 4, 10, zero);
    arm_pair(1, 0, red, green);

    farm_area.mode.place.location = PLACE_FARM;
    host_clear();
    draw_owned(&art_object, 20, 20, 192, 8, 8, 16, 16, 128, 128, 128);
    failed |= art_check(art_read(24, 24, 255, 0, 0), "the object model on the farm reads its picture");

    farm_area.mode.place.location = PLACE_4;
    host_clear();
    draw_owned(&art_object, 20, 40, 192, 8, 8, 16, 16, 128, 128, 128);
    failed |= art_check(art_read(24, 44, 255, 0, 0), "the object model in the coop reads its picture");

    farm_area.mode.place.location = PLACE_5;
    host_clear();
    draw_owned(&art_object, 20, 60, 192, 8, 8, 16, 16, 128, 128, 128);
    failed |= art_check(art_read(24, 64, 255, 0, 0), "the object model in the barn reads its picture");

    farm_area.mode.place.location = PLACE_FARM;
    host_clear();
    draw_owned(&art_character, 40, 20, 192, 8, 8, 16, 16, 128, 128, 128);
    failed |= art_check(art_read(44, 24, 0, 255, 0), "the character model reads its own picture");

    farm_area.mode.place.location = PLACE_2;
    host_clear();
    draw_owned(&art_object, 20, 80, 192, 8, 8, 16, 16, 128, 128, 128);
    failed |= art_check(art_read(24, 84, 13, 18, 31), "the object model outside its places keeps the cleared sample");

    farm_area.mode.place.location = PLACE_FARM;
    host_clear();
    draw_owned(&art_other, 60, 20, 192, 8, 8, 16, 16, 128, 128, 128);
    failed |= art_check(art_read(64, 24, 13, 18, 31), "a model with no rule keeps the cleared sample");

    host_clear();
    draw_owned(&art_object, 80, 20, 192, 40, 40, 48, 48, 128, 128, 128);
    failed |= art_check(art_read(84, 24, 13, 18, 31), "another polygon of the object model keeps the cleared sample");

    arm_pair(0, 0, zero, green);
    host_clear();
    draw_owned(&art_object, 100, 20, 192, 8, 8, 16, 16, 200, 40, 20);
    failed |= art_check(art_read(104, 24, 13, 18, 31), "a picture cell of word 0x0000 does not paint a substitute color");

    arm_pair(1, 0, green, green);
    base = SDL_GetBasePath();
    if (base == NULL) {
        model_art_test_end();
        farm_area.mode.place.location = saved;
        return art_check(0, "the executable folder is available");
    }
    snprintf(dir, sizeof dir, "%smods-model-art", base);
    SDL_free(base);
    _mkdir(dir);
    snprintf(path, sizeof path, "%s/192_0_8_8_16_16.bmp", dir);
    write_red(path);
    snprintf(art_env, sizeof art_env, "HMBTN_MODS=%s", dir);
    _putenv(art_env);
    mods_rescan();
    host_clear();
    draw_owned(&art_object, 120, 20, 192, 8, 8, 16, 16, 128, 128, 128);
    failed |= art_check(art_read(124, 24, 255, 0, 0), "a bmp for the object polygon reads the bmp color");
    _putenv("HMBTN_MODS=");
    mods_rescan();
    remove(path);
    _rmdir(dir);
    model_art_test_end();
    farm_area.mode.place.location = saved;
    host_clear();
    return failed == 0 ? 0 : 1;
}
