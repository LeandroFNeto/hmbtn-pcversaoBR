/* One frame of the ordering-table walk and the geometry calls behind each scene
 * object. Counters move only while a dump is collecting, and they do not change
 * what DrawOTag or the GTE return. */

#include <object_trace.h>

#include <SDL.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VIEW_X 160
#define VIEW_Y 120
#define HALF_X0 80
#define HALF_X1 240
#define HALF_Y0 60
#define HALF_Y1 180
#define GROUP_MAX 64
#define OPEN_MAX 20
#define TRI_MAX 16
#define DRAWN_MAX 1536
#define LOG_MAX 16384

typedef struct Tri {
    short x[3];
    short y[3];
    int covers;
} Tri;

typedef struct Group {
    int sx;
    int sy;
    int has_xy;
    int rtpt;
    int flag_fail;
    int nclip_fail;
    int nclip_pass;
    int tri_n;
    Tri tri[TRI_MAX];
} Group;

typedef struct Drawn {
    unsigned cmd;
    short x[4];
    short y[4];
    int n;
    int r;
    int g;
    int b;
    int textured;
    unsigned word;
    int covers;
} Drawn;

static int collecting;
static int key_collect;
static int f8_was;
static int force_enabled = -1;

static int poly_n;
static int rect_n;
static int line_n;
static int line_cut_n;
static int mask_n;
static int other_n;
static int walk_overflow;
static int group_overflow;

static Drawn *drawn;
static int drawn_n;

static Group *open_stack;
static int open_n;
static Group *done_group;
static int done_n;

/* 0 idle, 1 after rtpt, 2 after a front-facing stflg, 3 after nclip. */
static int phase;

static char *log_text;
static int log_len;

/* The tables are allocated once, when a dump starts, so they stay out of the
 * executable image. The walk itself does not allocate. */
static int ensure_tables(void) {
    if (drawn != NULL) {
        return 1;
    }
    drawn = (Drawn *)calloc((size_t)DRAWN_MAX, sizeof(Drawn));
    open_stack = (Group *)calloc((size_t)OPEN_MAX, sizeof(Group));
    done_group = (Group *)calloc((size_t)GROUP_MAX, sizeof(Group));
    log_text = (char *)calloc(1, LOG_MAX);
    if (drawn == NULL || open_stack == NULL || done_group == NULL || log_text == NULL) {
        return 0;
    }
    return 1;
}

static int env_enabled(void) {
    static int cached = -1;
    const char *env;

    if (cached < 0) {
        env = getenv("HMBTN_OT_DUMP");
        cached = env != NULL && strcmp(env, "1") == 0;
    }
    return cached;
}

static int dump_enabled(void) {
    if (force_enabled >= 0) {
        return force_enabled;
    }
    return env_enabled();
}

int object_trace_collecting(void) {
    return collecting;
}

static void reset_counters(void) {
    poly_n = 0;
    rect_n = 0;
    line_n = 0;
    line_cut_n = 0;
    mask_n = 0;
    other_n = 0;
    walk_overflow = 0;
    group_overflow = 0;
    drawn_n = 0;
    open_n = 0;
    done_n = 0;
    phase = 0;
}

static void add_line(const char *fmt, ...) {
    char buf[256];
    va_list ap;
    int n;

    va_start(ap, fmt);
    n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (n < 0) {
        return;
    }
    SDL_Log("%s", buf);
    if (log_text == NULL || log_len + n + 2 >= LOG_MAX) {
        return;
    }
    memcpy(log_text + log_len, buf, (size_t)n);
    log_len += n;
    log_text[log_len++] = '\n';
    log_text[log_len] = '\0';
}

static int contains_center(int x0, int y0, int x1, int y1, int x2, int y2) {
    long c1;
    long c2;
    long c3;

    c1 = (long)(x1 - x0) * (VIEW_Y - y0) - (long)(y1 - y0) * (VIEW_X - x0);
    c2 = (long)(x2 - x1) * (VIEW_Y - y1) - (long)(y2 - y1) * (VIEW_X - x1);
    c3 = (long)(x0 - x2) * (VIEW_Y - y2) - (long)(y0 - y2) * (VIEW_X - x2);
    if (c1 >= 0 && c2 >= 0 && c3 >= 0) {
        return 1;
    }
    if (c1 <= 0 && c2 <= 0 && c3 <= 0) {
        return 1;
    }
    return 0;
}

static int in_half(int x, int y) {
    return x >= HALF_X0 && x < HALF_X1 && y >= HALF_Y0 && y < HALF_Y1;
}

static int nearer(const Group *a, const Group *b) {
    int da;
    int db;

    da = (a->sx - VIEW_X) * (a->sx - VIEW_X) + (a->sy - VIEW_Y) * (a->sy - VIEW_Y);
    db = (b->sx - VIEW_X) * (b->sx - VIEW_X) + (b->sy - VIEW_Y) * (b->sy - VIEW_Y);
    return da < db;
}

static void fold_into(Group *dst, const Group *src) {
    int i;

    dst->rtpt += src->rtpt;
    dst->flag_fail += src->flag_fail;
    dst->nclip_fail += src->nclip_fail;
    dst->nclip_pass += src->nclip_pass;
    for (i = 0; i < src->tri_n && dst->tri_n < TRI_MAX; i++) {
        dst->tri[dst->tri_n++] = src->tri[i];
    }
}

static Group *current_group(void) {
    if (!collecting || open_n <= 0) {
        return NULL;
    }
    return &open_stack[open_n - 1];
}

static int verts_match(const Tri *tri, const Drawn *prim) {
    int i;

    if (prim->n < 3) {
        return 0;
    }
    for (i = 0; i < 3; i++) {
        if (prim->x[i] != tri->x[i] || prim->y[i] != tri->y[i]) {
            return 0;
        }
    }
    return 1;
}

static int tri_black(const Drawn *prim) {
    if (prim->r == 0 && prim->g == 0 && prim->b == 0) {
        return 1;
    }
    if (prim->textured && prim->word == 0) {
        return 1;
    }
    return 0;
}

static int group_was_drawn(const Group *g) {
    int t;
    int d;

    for (t = 0; t < g->tri_n; t++) {
        for (d = 0; d < drawn_n; d++) {
            if (verts_match(&g->tri[t], &drawn[d])) {
                return 1;
            }
        }
    }
    return 0;
}

/* The bed center sits outside the central half, so the half-window report can miss it. */
static void log_bed_group(void) {
    int best;
    int best_d;
    int i;

    best = -1;
    best_d = 0;
    for (i = 0; i < done_n; i++) {
        int dx;
        int dy;
        int d;

        if (!done_group[i].has_xy) {
            continue;
        }
        dx = done_group[i].sx - 254;
        dy = done_group[i].sy - 170;
        d = dx * dx + dy * dy;
        if (best < 0 || d < best_d) {
            best = i;
            best_d = d;
        }
    }
    if (best < 0) {
        add_line("trace object: bed 254,170 nearest none");
        return;
    }
    add_line("trace object: bed 254,170 nearest x %d y %d nclip_fail %d nclip_pass %d drawn %d", done_group[best].sx,
             done_group[best].sy, done_group[best].nclip_fail, done_group[best].nclip_pass,
             group_was_drawn(&done_group[best]));
}

static const char *outcome_name(const Group *g, int cover_black, int cover_any) {
    if (g->rtpt == 0) {
        return "never entered its primitive pass";
    }
    if (g->nclip_pass == 0) {
        return "geometry discard";
    }
    if (cover_black) {
        return "black emitted primitive";
    }
    if (!cover_any && (line_n > 0 || mask_n > 0)) {
        return "skipped line or mask command";
    }
    return "none";
}

static void flush_log(void) {
    int report[GROUP_MAX];
    int report_n;
    int i;
    int hit;
    int r0, g0, b0, r1, g1, b1, r2, g2, b2;
    int ar, ag, ab;

    report_n = 0;
    for (i = 0; i < done_n; i++) {
        if (done_group[i].has_xy && in_half(done_group[i].sx, done_group[i].sy)) {
            report[report_n++] = i;
        }
    }
    if (report_n == 0 && done_n > 0) {
        int best;

        best = -1;
        for (i = 0; i < done_n; i++) {
            if (!done_group[i].has_xy) {
                continue;
            }
            if (best < 0 || nearer(&done_group[i], &done_group[best])) {
                best = i;
            }
        }
        if (best >= 0) {
            report[report_n++] = best;
        }
    }

    add_line("trace object: walk poly %d rect %d line %d line_cut %d mask %d other %d overflow %d", poly_n, rect_n,
             line_n, line_cut_n, mask_n, other_n, walk_overflow);

    hit = -1;
    for (i = 0; i < drawn_n; i++) {
        if (drawn[i].covers) {
            hit = i;
        }
    }
    if (hit < 0) {
        add_line("trace object: center uncovered");
    } else {
        add_line("trace object: center cmd 0x%02x rgb %d,%d,%d word 0x%04x", drawn[hit].cmd, drawn[hit].r,
                 drawn[hit].g, drawn[hit].b, drawn[hit].word);
    }

    r0 = g0 = b0 = r1 = g1 = b1 = r2 = g2 = b2 = 0;
    ar = ag = ab = 0;
    gs_debug_flat_light(0, &r0, &g0, &b0);
    gs_debug_flat_light(1, &r1, &g1, &b1);
    gs_debug_flat_light(2, &r2, &g2, &b2);
    gs_debug_ambient(&ar, &ag, &ab);
    add_line("trace object: lights 0 %d,%d,%d 1 %d,%d,%d 2 %d,%d,%d ambient %d,%d,%d", r0, g0, b0, r1, g1, b1, r2,
             g2, b2, ar, ag, ab);
    if (group_overflow) {
        add_line("trace object: groups overflow");
    }
    log_bed_group();

    for (i = 0; i < report_n; i++) {
        Group *g;
        int t;
        int drawn_count;
        int cover_any;
        int cover_black;

        g = &done_group[report[i]];
        drawn_count = 0;
        cover_any = 0;
        cover_black = 0;
        for (t = 0; t < g->tri_n; t++) {
            int d;
            const Drawn *match;

            match = NULL;
            for (d = 0; d < drawn_n; d++) {
                if (verts_match(&g->tri[t], &drawn[d])) {
                    match = &drawn[d];
                }
            }
            if (match == NULL) {
                continue;
            }
            drawn_count++;
            if (g->tri[t].covers) {
                cover_any = 1;
                if (tri_black(match)) {
                    cover_black = 1;
                }
            }
        }
        add_line("trace object: group x %d y %d rtpt %d flag_fail %d nclip_fail %d nclip_pass %d drawn %d", g->sx,
                 g->sy, g->rtpt, g->flag_fail, g->nclip_fail, g->nclip_pass, drawn_count);
        add_line("trace object: outcome %s at %d,%d", outcome_name(g, cover_black, cover_any), g->sx, g->sy);
    }
}

void object_trace_present(int f8_down) {
    int enabled;

    if (!ensure_tables()) {
        return;
    }
    if (key_collect) {
        flush_log();
        collecting = 0;
        key_collect = 0;
    }
    enabled = dump_enabled();
    if (enabled && f8_down && !f8_was && !collecting) {
        reset_counters();
        collecting = 1;
        key_collect = 1;
    }
    f8_was = f8_down ? 1 : 0;
}

void object_trace_debug_set_enabled(int enabled) {
    force_enabled = enabled;
}

void object_trace_debug_reset(void) {
    if (!ensure_tables()) {
        return;
    }
    reset_counters();
    collecting = 0;
    key_collect = 0;
    f8_was = 0;
    force_enabled = -1;
    log_len = 0;
    log_text[0] = '\0';
}

void object_trace_debug_begin(void) {
    if (!ensure_tables()) {
        return;
    }
    reset_counters();
    log_len = 0;
    log_text[0] = '\0';
    collecting = 1;
    key_collect = 0;
}

void object_trace_debug_end(void) {
    if (!collecting || key_collect) {
        return;
    }
    flush_log();
    collecting = 0;
}

const char *object_trace_log(void) {
    if (log_text == NULL) {
        return "";
    }
    return log_text;
}

void object_trace_count_poly(void) {
    if (collecting) {
        poly_n++;
    }
}

void object_trace_count_rect(void) {
    if (collecting) {
        rect_n++;
    }
}

void object_trace_count_line(int cut) {
    if (!collecting) {
        return;
    }
    line_n++;
    if (cut) {
        line_cut_n++;
    }
}

void object_trace_count_other(int mask) {
    if (!collecting) {
        return;
    }
    other_n++;
    if (mask) {
        mask_n++;
    }
}

void object_trace_note_drawn(unsigned cmd, int n, const short *xy, int r, int g, int b, int textured, unsigned word,
                             int covers) {
    Drawn *d;
    int i;

    if (!collecting) {
        return;
    }
    if (drawn_n >= DRAWN_MAX) {
        walk_overflow = 1;
        return;
    }
    d = &drawn[drawn_n++];
    d->cmd = cmd;
    d->n = n > 4 ? 4 : n;
    d->r = r;
    d->g = g;
    d->b = b;
    d->textured = textured;
    d->word = word;
    d->covers = covers;
    for (i = 0; i < 4; i++) {
        d->x[i] = 0;
        d->y[i] = 0;
    }
    for (i = 0; i < d->n; i++) {
        d->x[i] = xy[i * 2];
        d->y[i] = xy[i * 2 + 1];
    }
}

void object_trace_push(void) {
    Group *g;

    if (!collecting || open_n >= OPEN_MAX) {
        return;
    }
    g = &open_stack[open_n++];
    memset(g, 0, sizeof(*g));
}

static int parent_is_bed_slot(void) {
    Group *parent;
    int dx;
    int dy;

    if (open_n <= 0) {
        return 0;
    }
    parent = &open_stack[open_n - 1];
    if (!parent->has_xy) {
        return 0;
    }
    dx = parent->sx - 301;
    dy = parent->sy - 156;
    return dx * dx + dy * dy <= 20 * 20;
}

void object_trace_pop(void) {
    Group *g;
    int bed_parent;

    if (!collecting || open_n <= 0) {
        return;
    }
    g = &open_stack[--open_n];
    bed_parent = parent_is_bed_slot();
    if (bed_parent) {
        fold_into(&open_stack[open_n - 1], g);
    }
    if (!g->has_xy) {
        if (open_n > 0 && !bed_parent) {
            fold_into(&open_stack[open_n - 1], g);
        }
        return;
    }
    if (done_n >= GROUP_MAX) {
        group_overflow = 1;
        return;
    }
    done_group[done_n++] = *g;
}

void object_trace_rtps(int sx, int sy) {
    Group *g;

    phase = 0;
    g = current_group();
    if (g == NULL || g->has_xy) {
        return;
    }
    g->sx = sx;
    g->sy = sy;
    g->has_xy = 1;
}

void object_trace_rtpt(void) {
    Group *g;

    g = current_group();
    if (g == NULL) {
        phase = 0;
        return;
    }
    g->rtpt++;
    phase = 1;
}

void object_trace_stflg(int value) {
    Group *g;

    if (phase != 1) {
        return;
    }
    g = current_group();
    if (g != NULL && value < 0) {
        g->flag_fail++;
    }
    phase = value < 0 ? 0 : 2;
}

void object_trace_stopz(int mac0, int x0, int y0, int x1, int y1, int x2, int y2) {
    Group *g;
    Tri *tri;

    if (phase != 3) {
        return;
    }
    phase = 0;
    g = current_group();
    if (g == NULL) {
        return;
    }
    if (mac0 <= 0) {
        g->nclip_fail++;
        return;
    }
    g->nclip_pass++;
    if (g->tri_n >= TRI_MAX) {
        return;
    }
    tri = &g->tri[g->tri_n++];
    tri->x[0] = (short)x0;
    tri->y[0] = (short)y0;
    tri->x[1] = (short)x1;
    tri->y[1] = (short)y1;
    tri->x[2] = (short)x2;
    tri->y[2] = (short)y2;
    tri->covers = contains_center(x0, y0, x1, y1, x2, y2);
}

/* nclip sits between stflg and stopz. The phase moves here so a stopz that did
 * not follow nclip is not counted. */
void object_trace_nclip(void) {
    if (phase == 2) {
        phase = 3;
    }
}
