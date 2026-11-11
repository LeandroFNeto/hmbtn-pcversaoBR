/* Loads item 0x064 through the game's picture call and remembers the cell.
 * The stored polygon row stays empty until a farm log fills it. */

#include <bin_picture.h>
#include <vram.h>

#include "farm_state.h"

#include <SDL.h>

#define BIN_PICTURE_ID 0x064
#define BIN_PICTURE_CLUT_RESERVED 1008

typedef struct BinRow {
    int set;
    int page_x;
    int page_y;
    int u0;
    int v0;
    int u1;
    int v1;
} BinRow;

typedef struct BinCell {
    int live;
    int page_x;
    int page_y;
    int depth;
    int clut_x;
    int clut_y;
    int u0;
    int v0;
    int u1;
    int v1;
} BinCell;

static BinRow game_row;
static BinCell loaded;
static short noted_place = -2;
static int test_on;
static BinRow test_row;
static BinCell test_cell;

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

static int place_armed(short place) {
    return place == PLACE_FARM || place == PLACE_4 || place == PLACE_5;
}

static int cell_page_x(int clut_x) {
    return clut_x >= BIN_PICTURE_CLUT_RESERVED ? 960 : 832;
}

static int words_nonzero(int page_x, int page_y, int u0, int v0, int u1, int v1, int depth) {
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
        for (u = u0; u < u1;) {
            if (depth <= 4) {
                x = page_x + u / 4;
                u += 4;
            } else if (depth <= 8) {
                x = page_x + u / 2;
                u += 2;
            } else {
                x = page_x + u;
                u += 1;
            }
            y = page_y + v;
            if (vram[y & 511][x & 1023] != 0) {
                return 1;
            }
        }
    }
    return 0;
}

static void cell_from_grid(BinCell *cell) {
    int row;
    int col;
    FarmGridEntry *entry;
    int u0;
    int v0;
    int u1;
    int v1;

    cell->live = 0;
    for (row = 0; row < PICTURE_ROWS; row++) {
        for (col = 0; col < PICTURE_SLOTS; col++) {
            if (farm_area.mode.pictureIds[row][col] != BIN_PICTURE_ID) {
                continue;
            }
            entry = &farm_area.mode.pictureEntries[row][col];
            cell->page_x = cell_page_x(entry->clutX);
            cell->page_y = 0;
            cell->depth = 4;
            cell->clut_x = entry->clutX;
            cell->clut_y = entry->clutY;
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
            cell->u0 = u0;
            cell->v0 = v0;
            cell->u1 = u1;
            cell->v1 = v1;
            cell->live = words_nonzero(cell->page_x, cell->page_y, u0, v0, u1, v1, 4);
            return;
        }
    }
}

void bin_picture_note_place(void) {
    short place;
    int called;

    place = farm_area.mode.place.location;
    if (!place_armed(place)) {
        if (noted_place != place) {
            noted_place = place;
            loaded.live = 0;
        }
        return;
    }
    if (noted_place == place) {
        return;
    }
    if (game_state.itemInfo == 0 || game_state.system.workSectorBusy != 0) {
        return;
    }
    noted_place = place;
    loaded.live = 0;
    if (picture_present(BIN_PICTURE_ID)) {
        called = 1;
    } else {
        called = picture_load_to_vram(BIN_PICTURE_ID);
    }
    if (called != 0) {
        cell_from_grid(&loaded);
    }
    SDL_Log("bin picture: place %d call %d live %d page %d,%d uv %d,%d %d,%d", (int)place, called, loaded.live,
            loaded.page_x, loaded.page_y, loaded.u0, loaded.v0, loaded.u1, loaded.v1);
}

static const BinRow *active_row(void) {
    if (test_on) {
        return &test_row;
    }
    return &game_row;
}

static const BinCell *active_cell(void) {
    if (test_on) {
        return &test_cell;
    }
    return &loaded;
}

int bin_picture_redirect(int page_x, int page_y, int u0, int v0, int u1, int v1, int replaced, int *out_page_x,
                         int *out_page_y, int *out_depth, int *out_clut_x, int *out_clut_y, int *out_u0, int *out_v0,
                         int *out_u1, int *out_v1) {
    const BinRow *row;
    const BinCell *cell;

    if (replaced || !place_armed(farm_area.mode.place.location)) {
        return 0;
    }
    row = active_row();
    cell = active_cell();
    if (!row->set || !cell->live) {
        return 0;
    }
    if (page_x != row->page_x || page_y != row->page_y || u0 != row->u0 || v0 != row->v0 || u1 != row->u1 ||
        v1 != row->v1) {
        return 0;
    }
    *out_page_x = cell->page_x;
    *out_page_y = cell->page_y;
    *out_depth = cell->depth;
    *out_clut_x = cell->clut_x;
    *out_clut_y = cell->clut_y;
    *out_u0 = cell->u0;
    *out_v0 = cell->v0;
    *out_u1 = cell->u1;
    *out_v1 = cell->v1;
    return 1;
}

void bin_picture_test_arm(int page_x, int page_y, int u0, int v0, int u1, int v1, int cell_page_x, int cell_page_y,
                          int depth, int clut_x, int clut_y, int cell_u0, int cell_v0, int cell_u1, int cell_v1,
                          int live) {
    test_on = 1;
    test_row.set = 1;
    test_row.page_x = page_x;
    test_row.page_y = page_y;
    test_row.u0 = u0;
    test_row.v0 = v0;
    test_row.u1 = u1;
    test_row.v1 = v1;
    test_cell.live = live;
    test_cell.page_x = cell_page_x;
    test_cell.page_y = cell_page_y;
    test_cell.depth = depth;
    test_cell.clut_x = clut_x;
    test_cell.clut_y = clut_y;
    test_cell.u0 = cell_u0;
    test_cell.v0 = cell_v0;
    test_cell.u1 = cell_u1;
    test_cell.v1 = cell_v1;
}

void bin_picture_test_disarm(void) {
    test_on = 0;
}
