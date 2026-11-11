#ifndef PC_PORT_BG_TRACE_H
#define PC_PORT_BG_TRACE_H

/* Armed by HMBTN_BG_TRACE=1. A rising F8 on a house scene or the flashback
 * records the primitives at the screen edges. The record does not recolor. */

typedef struct {
    int none;
    unsigned cmd;
    int r, g, b;
    int semi;
    int abr;
    int clear_after;
    int pixel_r, pixel_g, pixel_b;
} BgTracePoint;

typedef struct {
    int skipped;
    BgTracePoint point[5];
    int large_count;
    struct {
        unsigned cmd;
        int x0, y0, x1, y1;
        int r, g, b, semi, abr;
    } large[8];
    int clear_count;
    struct {
        int x, y, w, h, r, g, b, window;
    } clear[32];
    int e3_count, e4_count, e5_count, e6_count;
    unsigned e3_word, e4_word, e5_word, e6_word;
    int fill_count;
    int fill_x, fill_y, fill_w, fill_h, fill_r, fill_g, fill_b;
    int color_r, color_g, color_b, color_a;
    unsigned stencil_mask;
} BgTraceView;

void bg_trace_force(int force);
void bg_trace_reset(void);
int bg_trace_emit(int place, int scene);
void bg_trace_present(int f8_down);
const BgTraceView *bg_trace_last(void);
int bg_trace_skip_lines(void);
int bg_trace_edge_lines(void);
int bg_trace_logged_clears(void);
int bg_trace_self_check(void);

#endif
