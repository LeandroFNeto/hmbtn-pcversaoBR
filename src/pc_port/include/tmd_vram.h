#ifndef TMD_VRAM_H
#define TMD_VRAM_H

/* One farm frame, armed by HMBTN_TMD_VRAM=1. Rectangles are recorded from the
 * moment the farm scene is entered. A self-check turns recording on itself. */

int tmd_vram_collecting(void);

void tmd_vram_note_rect(int kind, int x, int y, int w, int h);
void tmd_vram_note_drawn(unsigned cmd, int n, const short *xy, int textured, int u, int v, int page_x,
                         int page_y, int clut_x, int clut_y, int depth);

/* Rising F8 dumps one farm frame that is not a fade. A short black blob dumps
 * the textured primitives that overlap it. A full-height blob, or a frame with
 * no blob, dumps the ones that cover the center of the view. No such primitive
 * leaves the dump armed. A held key does not dump again. */
void tmd_vram_present(int f8_down);

int tmd_vram_self_check(void);

#endif
