#ifndef PC_PORT_TEXTURE_DUMP_H
#define PC_PORT_TEXTURE_DUMP_H

/* One frame of textured primitives, armed by HMBTN_DUMP_TEXTURES=1.
 * A rising F8 writes each distinct page and UV bounds as
 * pageX_pageY_u0_v0_u1_v1.bmp under mods/textures/dump beside the executable.
 * Holding F8 does not write the next frame. The variable unset writes nothing. */

void texture_dump_note(int page_x, int page_y, int depth, int clut_x, int clut_y, int u0, int v0, int u1, int v1);
void texture_dump_present(int f8_down);
int texture_dump_self_check(void);

#endif
