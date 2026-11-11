#ifndef MODEL_ART_H
#define MODEL_ART_H

/* A look rule points one polygon of one model at an item picture.
 * The vertices stay the ones the model produced. */

void model_art_note_place(void);

/* 1 when this packet's model and its page and UV equal a rule, the area is one
 * that rule names, and the picture has a texel. `replaced` means a BMP already
 * matches, and then this returns 0. */
int model_art_redirect(const void *packet, int page_x, int page_y, int u0, int v0, int u1, int v1, int replaced,
                       int *out_page_x, int *out_page_y, int *out_depth, int *out_clut_x, int *out_clut_y, int *out_u0,
                       int *out_v0, int *out_u1, int *out_v1);

/* Constructed rules for the self-check. The shipped rows stay empty. */
void model_art_test_begin(void);
void model_art_test_add(const void *model, const short *places, int nplaces, int page_x, int page_y, int u0, int v0,
                        int u1, int v1, int cell_page_x, int cell_page_y, int depth, int clut_x, int clut_y, int cell_u0,
                        int cell_v0, int cell_u1, int cell_v1, int live);
void model_art_test_end(void);

int model_art_self_check(void);

#endif
