#ifndef OBJECT_TRACE_H
#define OBJECT_TRACE_H

/* One presented frame of DrawOTag and the libgte calls that feed scene objects.
 * Armed by HMBTN_OT_DUMP=1 and F8. Recording is off unless a dump is collecting. */

int object_trace_collecting(void);

/* Host present: flush a collection that is already running, then arm the next
 * frame on a rising F8 edge when the dump switch is on. */
void object_trace_present(int f8_down);

/* Self-check. force_enabled -1 uses the environment variable. */
void object_trace_debug_set_enabled(int force_enabled);
void object_trace_debug_reset(void);
void object_trace_debug_begin(void);
void object_trace_debug_end(void);
const char *object_trace_log(void);

void object_trace_count_poly(void);
void object_trace_count_rect(void);
void object_trace_count_line(int cut);
void object_trace_count_other(int mask);

/* xy is 4 pairs. covers is 1 when the primitive contains (160, 120). word is the
 * sampled texel there, and is meaningful when textured is 1. */
void object_trace_note_drawn(unsigned cmd, int n, const short *xy, int r, int g, int b, int textured,
                             unsigned word, int covers);

void object_trace_push(void);
void object_trace_pop(void);
void object_trace_rtps(int sx, int sy);
void object_trace_rtpt(void);
void object_trace_stflg(int value);
void object_trace_nclip(void);
void object_trace_stopz(int mac0, int x0, int y0, int x1, int y1, int x2, int y2);

/* Last values GsSetFlatLight and GsSetAmbient stored. Returns 0 when that slot
 * has never been written. */
int gs_debug_flat_light(int id, int *r, int *g, int *b);
int gs_debug_flat_dir(int id, int *vx, int *vy, int *vz);
int gs_debug_ambient(int *r, int *g, int *b);
int gs_debug_light_mode(int *mode);

int object_trace_self_check(void);

#endif
