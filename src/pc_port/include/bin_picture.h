#ifndef BIN_PICTURE_H
#define BIN_PICTURE_H

/* The shipping bin's item picture. The model draw is unchanged: one polygon
 * can sample the cell picture_load_to_vram already filled. */

void bin_picture_note_place(void);

/* 1 when this primitive's page and UV equal the stored bin row, the current
 * area is the farm, the coop, or the barn, and the picture cell has a texel.
 * Writes that cell's page, depth, CLUT, and UV. `replaced` means a BMP already
 * matches, and then this returns 0. */
int bin_picture_redirect(int page_x, int page_y, int u0, int v0, int u1, int v1, int replaced, int *out_page_x,
                         int *out_page_y, int *out_depth, int *out_clut_x, int *out_clut_y, int *out_u0, int *out_v0,
                         int *out_u1, int *out_v1);

/* A constructed row and cell for the self-check. The game row stays empty. */
void bin_picture_test_arm(int page_x, int page_y, int u0, int v0, int u1, int v1, int cell_page_x, int cell_page_y,
                          int depth, int clut_x, int clut_y, int cell_u0, int cell_v0, int cell_u1, int cell_v1,
                          int live);
void bin_picture_test_disarm(void);

#endif
