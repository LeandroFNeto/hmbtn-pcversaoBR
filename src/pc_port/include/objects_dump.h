#ifndef OBJECTS_DUMP_H
#define OBJECTS_DUMP_H

/* One farm frame, armed by HMBTN_OBJECTS=1. Recording is off unless that switch
 * is set or a self-check is inside objects_dump_debug_begin. */

int objects_dump_collecting(void);

void objects_dump_note_drawn(unsigned cmd, int n, const short *xy, int r, int g, int b, int textured, int u,
                             int v, int page_x, int page_y, int clut_x, int clut_y, int depth);
/* next is the following command byte, or -1 when the packet ends at the line. */
void objects_dump_note_skip(unsigned cmd, int next);
void objects_dump_note_flag(int value);
void objects_dump_note_nclip(int mac0);

/* End of a presented frame. With HMBTN_OBJECTS=1, each rising F8 dumps that farm
 * frame when it is not a fade. A held key does not dump again. Frames before the
 * key are discarded, so loading the farm does not consume a dump. */
void objects_dump_present(int f8_down);

void objects_dump_debug_begin(void);
/* 1 when this frame was dumped. A fade returns 0 and leaves the log empty. */
int objects_dump_debug_capture(void);
const char *objects_dump_log(void);
int objects_dump_self_check(void);

#endif
