#ifndef PC_PORT_MODEL_TEX_H
#define PC_PORT_MODEL_TEX_H

/* The TMD header live while model_draw or model_draw_visible builds packets.
 * DrawOTag runs later, so the header is stored on the primitive-buffer range. */

void model_tex_begin(const void *tmd);
void model_tex_end(void);
void model_tex_hook(int code);
void model_tex_finish(void);
const void *model_tex_lookup(const void *packet);
void model_tex_drop_below(const void *cursor);
int model_tex_self_check(void);

#endif
