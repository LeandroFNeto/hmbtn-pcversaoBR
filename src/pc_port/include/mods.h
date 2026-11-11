#ifndef PC_PORT_MODS_H
#define PC_PORT_MODS_H

/* BMP overlays for one page and UV bounds. 0 means the primitive samples VRAM. */

void mods_rescan(void);
void mods_shutdown(void);
unsigned mods_lookup(int page_x, int page_y, int u0, int v0, int u1, int v1);
unsigned mods_lookup_model(const void *tmd);

#endif
