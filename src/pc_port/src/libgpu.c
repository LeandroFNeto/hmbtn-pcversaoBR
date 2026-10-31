/* The host GPU: DrawOTag walks an ordering table, runs its transfers and draw-mode
 * words, and draws its polygons and rectangles with OpenGL, sampling the host VRAM
 * (vram.c) through one 1024 x 512 texture. */

#include <libgpu.h>
#include <ps1_memory.h>
#include <vram.h>

#include <SDL.h>
#include <GL/gl.h>

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#ifndef APIENTRY
#define APIENTRY
#endif

#define GL_FRAGMENT_SHADER 0x8B30
#define GL_VERTEX_SHADER 0x8B31
#define GL_COMPILE_STATUS 0x8B81
#define GL_LINK_STATUS 0x8B82
#define GL_ARRAY_BUFFER 0x8892
#define GL_DYNAMIC_DRAW 0x88E8
#define GL_INFO_LOG_LENGTH 0x8B84
#define GL_CLAMP_TO_EDGE 0x812F
#define GL_FUNC_ADD 0x8006
#define GL_FUNC_REVERSE_SUBTRACT 0x800B

typedef char GLchar;
typedef ptrdiff_t GLsizeiptr;

typedef GLuint(APIENTRY *PFN_glCreateShader)(GLenum);
typedef void(APIENTRY *PFN_glShaderSource)(GLuint, GLsizei, const GLchar *const *, const GLint *);
typedef void(APIENTRY *PFN_glCompileShader)(GLuint);
typedef void(APIENTRY *PFN_glGetShaderiv)(GLuint, GLenum, GLint *);
typedef void(APIENTRY *PFN_glGetShaderInfoLog)(GLuint, GLsizei, GLsizei *, GLchar *);
typedef GLuint(APIENTRY *PFN_glCreateProgram)(void);
typedef void(APIENTRY *PFN_glAttachShader)(GLuint, GLuint);
typedef void(APIENTRY *PFN_glLinkProgram)(GLuint);
typedef void(APIENTRY *PFN_glGetProgramiv)(GLuint, GLenum, GLint *);
typedef void(APIENTRY *PFN_glGetProgramInfoLog)(GLuint, GLsizei, GLsizei *, GLchar *);
typedef void(APIENTRY *PFN_glUseProgram)(GLuint);
typedef void(APIENTRY *PFN_glDeleteShader)(GLuint);
typedef void(APIENTRY *PFN_glDeleteProgram)(GLuint);
typedef void(APIENTRY *PFN_glGenBuffers)(GLsizei, GLuint *);
typedef void(APIENTRY *PFN_glDeleteBuffers)(GLsizei, const GLuint *);
typedef void(APIENTRY *PFN_glBindBuffer)(GLenum, GLuint);
typedef void(APIENTRY *PFN_glBufferData)(GLenum, GLsizeiptr, const void *, GLenum);
typedef void(APIENTRY *PFN_glEnableVertexAttribArray)(GLuint);
typedef void(APIENTRY *PFN_glVertexAttribPointer)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void *);
typedef GLint(APIENTRY *PFN_glGetAttribLocation)(GLuint, const GLchar *);
typedef GLint(APIENTRY *PFN_glGetUniformLocation)(GLuint, const GLchar *);
typedef void(APIENTRY *PFN_glUniform1i)(GLint, GLint);
typedef void(APIENTRY *PFN_glBlendEquation)(GLenum);

static PFN_glCreateShader p_glCreateShader;
static PFN_glShaderSource p_glShaderSource;
static PFN_glCompileShader p_glCompileShader;
static PFN_glGetShaderiv p_glGetShaderiv;
static PFN_glGetShaderInfoLog p_glGetShaderInfoLog;
static PFN_glCreateProgram p_glCreateProgram;
static PFN_glAttachShader p_glAttachShader;
static PFN_glLinkProgram p_glLinkProgram;
static PFN_glGetProgramiv p_glGetProgramiv;
static PFN_glGetProgramInfoLog p_glGetProgramInfoLog;
static PFN_glUseProgram p_glUseProgram;
static PFN_glDeleteShader p_glDeleteShader;
static PFN_glDeleteProgram p_glDeleteProgram;
static PFN_glGenBuffers p_glGenBuffers;
static PFN_glDeleteBuffers p_glDeleteBuffers;
static PFN_glBindBuffer p_glBindBuffer;
static PFN_glBufferData p_glBufferData;
static PFN_glEnableVertexAttribArray p_glEnableVertexAttribArray;
static PFN_glVertexAttribPointer p_glVertexAttribPointer;
static PFN_glGetAttribLocation p_glGetAttribLocation;
static PFN_glGetUniformLocation p_glGetUniformLocation;
static PFN_glUniform1i p_glUniform1i;
static PFN_glBlendEquation p_glBlendEquation;

static GLuint gpu_program;
static GLuint gpu_vbo;
static GLuint gpu_vram_tex;
static int gpu_ready;

/* One vertex: screen position, packet color (0..1), texel UV inside the page, the
 * page (x, y, depth 0 = untextured, 4, 8, 15), the CLUT, and (raw, blend mode or -1). */
typedef struct GpuVert {
    float x, y;
    float r, g, b;
    float u, v;
    float page_x, page_y, depth;
    float clut_x, clut_y;
    float raw, mode;
} GpuVert;

enum { ATTR_POS, ATTR_COLOR, ATTR_UV, ATTR_PAGE, ATTR_CLUT, ATTR_MISC, ATTR_COUNT };
static const char *const attr_names[ATTR_COUNT] = {"aPos", "aColor", "aUV", "aPage", "aClut", "aMisc"};
static const int attr_sizes[ATTR_COUNT] = {2, 3, 2, 3, 2, 2};
static GLint attr_loc[ATTR_COUNT];

#define GPU_VERT_MAX 6144
static GpuVert gpu_verts[GPU_VERT_MAX];
static int gpu_vert_count;
static int batch_subtract;

static float clear_rgb[3] = {13.0f / 255.0f, 18.0f / 255.0f, 31.0f / 255.0f};

/* The GPU draw mode, which persists across command lists as on the PS1. */
static int st_page_x;
static int st_page_y;
static int st_depth = 4;
static int st_abr;

/* A TILE (0x60) the walker must skip, then the flat quad that is drawn. */
static TILE demo_skip;
static POLY_F4 demo_quad;

static const char *VERT_SRC =
    "#version 120\n"
    "attribute vec2 aPos;\n"
    "attribute vec3 aColor;\n"
    "attribute vec2 aUV;\n"
    "attribute vec3 aPage;\n"
    "attribute vec2 aClut;\n"
    "attribute vec2 aMisc;\n"
    "varying vec3 vColor;\n"
    "varying vec2 vUV;\n"
    "varying vec3 vPage;\n"
    "varying vec2 vClut;\n"
    "varying vec2 vMisc;\n"
    "void main() {\n"
    "    float x = (aPos.x / 320.0) * 2.0 - 1.0;\n"
    "    float y = 1.0 - (aPos.y / 240.0) * 2.0;\n"
    "    gl_Position = vec4(x, y, 0.0, 1.0);\n"
    "    vColor = aColor;\n"
    "    vUV = aUV;\n"
    "    vPage = aPage;\n"
    "    vClut = aClut;\n"
    "    vMisc = aMisc;\n"
    "}\n";

static const char *FRAG_SRC =
    "#version 120\n"
    "uniform sampler2D uVram;\n"
    "varying vec3 vColor;\n"
    "varying vec2 vUV;\n"
    "varying vec3 vPage;\n"
    "varying vec2 vClut;\n"
    "varying vec2 vMisc;\n"
    "float vram_word(vec2 p) {\n"
    "    vec4 t = texture2D(uVram, (mod(floor(p), vec2(1024.0, 512.0)) + 0.5) / vec2(1024.0, 512.0));\n"
    "    return floor(t.r * 255.0 + 0.5) + floor(t.a * 255.0 + 0.5) * 256.0;\n"
    "}\n"
    "void main() {\n"
    "    vec3 rgb = vColor;\n"
    "    float stp = 1.0;\n"
    "    if (vPage.z > 0.5) {\n"
    "        vec2 uv = mod(floor(vUV), 256.0);\n"
    "        float c;\n"
    "        if (vPage.z < 5.0) {\n"
    "            float w = vram_word(vec2(vPage.x + floor(uv.x / 4.0), vPage.y + uv.y));\n"
    "            float s = mod(uv.x, 4.0);\n"
    "            float d = s < 0.5 ? 1.0 : (s < 1.5 ? 16.0 : (s < 2.5 ? 256.0 : 4096.0));\n"
    "            float index = mod(floor(w / d), 16.0);\n"
    "            c = vram_word(vec2(vClut.x + index, vClut.y));\n"
    "        } else if (vPage.z < 9.0) {\n"
    "            float w = vram_word(vec2(vPage.x + floor(uv.x / 2.0), vPage.y + uv.y));\n"
    "            float index = mod(uv.x, 2.0) < 0.5 ? mod(w, 256.0) : floor(w / 256.0);\n"
    "            c = vram_word(vec2(vClut.x + index, vClut.y));\n"
    "        } else {\n"
    "            c = vram_word(vec2(vPage.x + uv.x, vPage.y + uv.y));\n"
    "        }\n"
    "        if (c < 0.5) {\n"
    "            discard;\n"
    "        }\n"
    "        rgb = vec3(mod(c, 32.0), mod(floor(c / 32.0), 32.0), mod(floor(c / 1024.0), 32.0)) / 31.0;\n"
    "        stp = floor(c / 32768.0);\n"
    "        if (vMisc.x < 0.5) {\n"
    "            rgb = min(rgb * vColor * (255.0 / 128.0), vec3(1.0));\n"
    "        }\n"
    "    }\n"
    "    vec4 o = vec4(rgb, 1.0);\n"
    "    if (vMisc.y > -0.5 && stp > 0.5) {\n"
    "        if (vMisc.y < 0.5) {\n"
    "            o = vec4(rgb * 0.5, 0.5);\n"
    "        } else if (vMisc.y < 2.5) {\n"
    "            o = vec4(rgb, 0.0);\n"
    "        } else {\n"
    "            o = vec4(rgb * 0.25, 0.0);\n"
    "        }\n"
    "    }\n"
    "    gl_FragColor = o;\n"
    "}\n";

static int compile_shader(GLenum type, const char *src, GLuint *out) {
    GLuint shader;
    GLint ok;
    char log[512];

    shader = p_glCreateShader(type);
    p_glShaderSource(shader, 1, &src, NULL);
    p_glCompileShader(shader);
    p_glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        log[0] = 0;
        p_glGetShaderInfoLog(shader, (GLsizei)sizeof(log), NULL, log);
        SDL_Log("shader compile failed: %s", log);
        p_glDeleteShader(shader);
        return -1;
    }
    *out = shader;
    return 0;
}

static void *load_proc(const char *name, int *missing) {
    void *proc;

    proc = SDL_GL_GetProcAddress(name);
    if (proc == NULL) {
        SDL_Log("missing GL entry %s", name);
        *missing = 1;
    }
    return proc;
}

int gpu_gl_init(void) {
    GLuint vs;
    GLuint fs;
    GLint ok;
    char log[512];
    int missing;
    int i;

    missing = 0;
    p_glCreateShader = (PFN_glCreateShader)load_proc("glCreateShader", &missing);
    p_glShaderSource = (PFN_glShaderSource)load_proc("glShaderSource", &missing);
    p_glCompileShader = (PFN_glCompileShader)load_proc("glCompileShader", &missing);
    p_glGetShaderiv = (PFN_glGetShaderiv)load_proc("glGetShaderiv", &missing);
    p_glGetShaderInfoLog = (PFN_glGetShaderInfoLog)load_proc("glGetShaderInfoLog", &missing);
    p_glCreateProgram = (PFN_glCreateProgram)load_proc("glCreateProgram", &missing);
    p_glAttachShader = (PFN_glAttachShader)load_proc("glAttachShader", &missing);
    p_glLinkProgram = (PFN_glLinkProgram)load_proc("glLinkProgram", &missing);
    p_glGetProgramiv = (PFN_glGetProgramiv)load_proc("glGetProgramiv", &missing);
    p_glGetProgramInfoLog = (PFN_glGetProgramInfoLog)load_proc("glGetProgramInfoLog", &missing);
    p_glUseProgram = (PFN_glUseProgram)load_proc("glUseProgram", &missing);
    p_glDeleteShader = (PFN_glDeleteShader)load_proc("glDeleteShader", &missing);
    p_glDeleteProgram = (PFN_glDeleteProgram)load_proc("glDeleteProgram", &missing);
    p_glGenBuffers = (PFN_glGenBuffers)load_proc("glGenBuffers", &missing);
    p_glDeleteBuffers = (PFN_glDeleteBuffers)load_proc("glDeleteBuffers", &missing);
    p_glBindBuffer = (PFN_glBindBuffer)load_proc("glBindBuffer", &missing);
    p_glBufferData = (PFN_glBufferData)load_proc("glBufferData", &missing);
    p_glEnableVertexAttribArray = (PFN_glEnableVertexAttribArray)load_proc("glEnableVertexAttribArray", &missing);
    p_glVertexAttribPointer = (PFN_glVertexAttribPointer)load_proc("glVertexAttribPointer", &missing);
    p_glGetAttribLocation = (PFN_glGetAttribLocation)load_proc("glGetAttribLocation", &missing);
    p_glGetUniformLocation = (PFN_glGetUniformLocation)load_proc("glGetUniformLocation", &missing);
    p_glUniform1i = (PFN_glUniform1i)load_proc("glUniform1i", &missing);
    p_glBlendEquation = (PFN_glBlendEquation)load_proc("glBlendEquation", &missing);
    if (missing) {
        return -1;
    }

    if (compile_shader(GL_VERTEX_SHADER, VERT_SRC, &vs) != 0) {
        return -1;
    }
    if (compile_shader(GL_FRAGMENT_SHADER, FRAG_SRC, &fs) != 0) {
        p_glDeleteShader(vs);
        return -1;
    }
    gpu_program = p_glCreateProgram();
    p_glAttachShader(gpu_program, vs);
    p_glAttachShader(gpu_program, fs);
    p_glLinkProgram(gpu_program);
    p_glDeleteShader(vs);
    p_glDeleteShader(fs);
    p_glGetProgramiv(gpu_program, GL_LINK_STATUS, &ok);
    if (!ok) {
        log[0] = 0;
        p_glGetProgramInfoLog(gpu_program, (GLsizei)sizeof(log), NULL, log);
        SDL_Log("program link failed: %s", log);
        p_glDeleteProgram(gpu_program);
        gpu_program = 0;
        return -1;
    }
    for (i = 0; i < ATTR_COUNT; i++) {
        attr_loc[i] = p_glGetAttribLocation(gpu_program, attr_names[i]);
        if (attr_loc[i] < 0) {
            SDL_Log("shader attribute %s missing", attr_names[i]);
            return -1;
        }
    }
    p_glUseProgram(gpu_program);
    p_glUniform1i(p_glGetUniformLocation(gpu_program, "uVram"), 0);
    p_glGenBuffers(1, &gpu_vbo);

    glGenTextures(1, &gpu_vram_tex);
    glBindTexture(GL_TEXTURE_2D, gpu_vram_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE_ALPHA, VRAM_WIDTH, VRAM_HEIGHT, 0, GL_LUMINANCE_ALPHA,
                 GL_UNSIGNED_BYTE, vram);
    {
        int y0;
        int y1;

        vram_take_dirty_rows(&y0, &y1);
    }

    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    p_glBlendEquation(GL_FUNC_ADD);
    gpu_ready = 1;
    return 0;
}

void gpu_gl_shutdown(void) {
    if (!gpu_ready) {
        return;
    }
    glDeleteTextures(1, &gpu_vram_tex);
    p_glDeleteBuffers(1, &gpu_vbo);
    p_glUseProgram(0);
    p_glDeleteProgram(gpu_program);
    gpu_vram_tex = 0;
    gpu_vbo = 0;
    gpu_program = 0;
    gpu_ready = 0;
    SDL_Log("GPU shut down");
}

static void upload_vram(void) {
    int y0;
    int y1;

    if (!vram_take_dirty_rows(&y0, &y1)) {
        return;
    }
    glBindTexture(GL_TEXTURE_2D, gpu_vram_tex);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, y0, VRAM_WIDTH, y1 - y0 + 1, GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE,
                    &vram[y0][0]);
}

static void flush_verts(void) {
    size_t offset;
    int i;

    if (!gpu_ready || gpu_vert_count == 0) {
        gpu_vert_count = 0;
        return;
    }
    upload_vram();
    p_glUseProgram(gpu_program);
    glBindTexture(GL_TEXTURE_2D, gpu_vram_tex);
    p_glBlendEquation(batch_subtract ? GL_FUNC_REVERSE_SUBTRACT : GL_FUNC_ADD);
    p_glBindBuffer(GL_ARRAY_BUFFER, gpu_vbo);
    p_glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(gpu_vert_count * (int)sizeof(GpuVert)), gpu_verts,
                   GL_DYNAMIC_DRAW);
    offset = 0;
    for (i = 0; i < ATTR_COUNT; i++) {
        p_glEnableVertexAttribArray((GLuint)attr_loc[i]);
        p_glVertexAttribPointer((GLuint)attr_loc[i], attr_sizes[i], GL_FLOAT, GL_FALSE, (GLsizei)sizeof(GpuVert),
                                (const void *)offset);
        offset += sizeof(float) * (size_t)attr_sizes[i];
    }
    glDrawArrays(GL_TRIANGLES, 0, gpu_vert_count);
    p_glBlendEquation(GL_FUNC_ADD);
    gpu_vert_count = 0;
}

/* A primitive as the walker decodes it: up to four corners and their shared state. */
typedef struct {
    float x, y, r, g, b, u, v;
} PrimVert;

typedef struct {
    PrimVert v[4];
    int count;
    float page_x, page_y, depth, clut_x, clut_y, raw, mode;
} Prim;

static void emit_vert(const Prim *p, int i) {
    GpuVert *out;

    out = &gpu_verts[gpu_vert_count++];
    out->x = p->v[i].x;
    out->y = p->v[i].y;
    out->r = p->v[i].r;
    out->g = p->v[i].g;
    out->b = p->v[i].b;
    out->u = p->v[i].u;
    out->v = p->v[i].v;
    out->page_x = p->page_x;
    out->page_y = p->page_y;
    out->depth = p->depth;
    out->clut_x = p->clut_x;
    out->clut_y = p->clut_y;
    out->raw = p->raw;
    out->mode = p->mode;
}

static void emit_prim(const Prim *p) {
    int subtract;

    subtract = p->mode > 1.5f && p->mode < 2.5f;
    if (subtract != batch_subtract || gpu_vert_count + 6 > GPU_VERT_MAX) {
        flush_verts();
        batch_subtract = subtract;
    }
    emit_vert(p, 0);
    emit_vert(p, 1);
    emit_vert(p, 2);
    if (p->count == 4) {
        emit_vert(p, 1);
        emit_vert(p, 3);
        emit_vert(p, 2);
    }
}

static void set_tpage(unsigned tp) {
    unsigned d;

    st_page_x = (int)(tp & 0xF) * 64;
    st_page_y = (int)((tp >> 4) & 1) * 256;
    st_abr = (int)((tp >> 5) & 3);
    d = (tp >> 7) & 3;
    st_depth = d == 0 ? 4 : (d == 1 ? 8 : 15);
}

static void set_color(PrimVert *v, u_long c) {
    v->r = (float)(c & 0xFF) / 255.0f;
    v->g = (float)((c >> 8) & 0xFF) / 255.0f;
    v->b = (float)((c >> 16) & 0xFF) / 255.0f;
}

static void set_xy(PrimVert *v, u_long xy) {
    v->x = (float)(short)(xy & 0xFFFF);
    v->y = (float)(short)(xy >> 16);
}

static void set_texture_state(Prim *p, int textured, int raw, int semi, unsigned clut) {
    p->depth = textured ? (float)st_depth : 0.0f;
    p->page_x = (float)st_page_x;
    p->page_y = (float)st_page_y;
    p->clut_x = (float)((clut & 0x3F) * 16);
    p->clut_y = (float)((clut >> 6) & 0x1FF);
    p->raw = raw ? 1.0f : 0.0f;
    p->mode = semi ? (float)st_abr : -1.0f;
}

static void resolve_texture(const Prim *p) {
    float u0;
    float v0;
    float u1;
    float v1;
    int i;

    u0 = u1 = p->v[0].u;
    v0 = v1 = p->v[0].v;
    for (i = 1; i < p->count; i++) {
        u0 = p->v[i].u < u0 ? p->v[i].u : u0;
        v0 = p->v[i].v < v0 ? p->v[i].v : v0;
        u1 = p->v[i].u > u1 ? p->v[i].u : u1;
        v1 = p->v[i].v > v1 ? p->v[i].v : v1;
    }
    texture_resolve((int)p->page_x, (int)p->page_y, (int)p->depth, (int)p->clut_x, (int)p->clut_y, (int)u0,
                    (int)v0, (int)u1, (int)v1);
}

/* Polygon commands 0x20-0x3F. Returns 0 when the packet is too short for its layout. */
static int decode_poly(const u_long *w, int avail, int emit) {
    unsigned cmd;
    int textured;
    int quad;
    int gouraud;
    int n;
    int need;
    int i;
    int k;
    unsigned clut;
    u_long color;
    Prim p;

    cmd = (unsigned)(w[0] >> 24);
    textured = (cmd & 0x04) != 0;
    quad = (cmd & 0x08) != 0;
    gouraud = (cmd & 0x10) != 0;
    n = quad ? 4 : 3;
    need = 1 + n * (1 + textured) + (gouraud ? n - 1 : 0);
    if (need > avail) {
        return 0;
    }
    if (!emit) {
        return 1;
    }
    memset(&p, 0, sizeof(p));
    p.count = n;
    clut = 0;
    color = w[0];
    i = 1;
    for (k = 0; k < n; k++) {
        if (gouraud && k > 0) {
            color = w[i++];
        }
        set_color(&p.v[k], color);
        set_xy(&p.v[k], w[i++]);
        if (textured) {
            u_long uvw = w[i++];

            p.v[k].u = (float)(uvw & 0xFF);
            p.v[k].v = (float)((uvw >> 8) & 0xFF);
            if (k == 0) {
                clut = (unsigned)(uvw >> 16);
            } else if (k == 1) {
                set_tpage((unsigned)(uvw >> 16));
            }
        }
    }
    set_texture_state(&p, textured, cmd & 0x01, cmd & 0x02, clut);
    if (textured) {
        resolve_texture(&p);
    }
    emit_prim(&p);
    return 1;
}

/* Rectangle commands 0x60-0x7F. */
static int decode_rect(const u_long *w, int avail, int emit) {
    static const int fixed[4] = {0, 1, 8, 16};
    unsigned cmd;
    int textured;
    int size;
    int need;
    int i;
    int rw;
    int rh;
    unsigned clut;
    float u0;
    float v0;
    Prim p;

    cmd = (unsigned)(w[0] >> 24);
    textured = (cmd & 0x04) != 0;
    size = (int)((cmd >> 3) & 3);
    need = 2 + textured + (size == 0);
    if (need > avail) {
        return 0;
    }
    if (!emit) {
        return 1;
    }
    memset(&p, 0, sizeof(p));
    p.count = 4;
    set_color(&p.v[0], w[0]);
    set_xy(&p.v[0], w[1]);
    i = 2;
    clut = 0;
    u0 = 0.0f;
    v0 = 0.0f;
    if (textured) {
        u0 = (float)(w[i] & 0xFF);
        v0 = (float)((w[i] >> 8) & 0xFF);
        clut = (unsigned)(w[i] >> 16);
        i++;
    }
    if (size == 0) {
        rw = (int)(w[i] & 0x3FF);
        rh = (int)((w[i] >> 16) & 0x1FF);
    } else {
        rw = rh = fixed[size];
    }
    for (i = 1; i < 4; i++) {
        p.v[i] = p.v[0];
    }
    p.v[1].x += (float)rw;
    p.v[2].y += (float)rh;
    p.v[3].x += (float)rw;
    p.v[3].y += (float)rh;
    p.v[0].u = p.v[2].u = u0;
    p.v[1].u = p.v[3].u = u0 + (float)rw;
    p.v[0].v = p.v[1].v = v0;
    p.v[2].v = p.v[3].v = v0 + (float)rh;
    set_texture_state(&p, textured, cmd & 0x01, cmd & 0x02, clut);
    if (textured) {
        resolve_texture(&p);
    }
    emit_prim(&p);
    return 1;
}

/* Runs one packet's command words. Returns 1 when it drew a primitive. */
static int run_packet(const u_long *w, int len, int emit) {
    int i;
    unsigned cmd;

    i = 0;
    while (i < len) {
        cmd = (unsigned)(w[i] >> 24);
        if (cmd >= 0x20 && cmd <= 0x3F) {
            return decode_poly(w + i, len - i, emit);
        }
        if (cmd >= 0x60 && cmd <= 0x7F) {
            return decode_rect(w + i, len - i, emit);
        }
        if (cmd >= 0x40 && cmd <= 0x5F) {
            return 0;
        }
        if (cmd == 0xA0) {
            int x;
            int y;
            int rw;
            int rh;
            int words;

            if (i + 2 >= len) {
                return 0;
            }
            x = (int)(w[i + 1] & 0xFFFF);
            y = (int)(w[i + 1] >> 16);
            rw = (int)(w[i + 2] & 0xFFFF);
            rh = (int)(w[i + 2] >> 16);
            words = (rw * rh + 1) / 2;
            if (i + 3 + words > len) {
                return 0;
            }
            if (emit) {
                flush_verts();
                vram_load(x, y, rw, rh, (const uint16_t *)&w[i + 3]);
            }
            i += 3 + words;
            continue;
        }
        if (cmd == 0x80) {
            if (i + 3 >= len) {
                return 0;
            }
            if (emit) {
                flush_verts();
                vram_move((int)(w[i + 1] & 0xFFFF), (int)(w[i + 1] >> 16), (int)(w[i + 2] & 0xFFFF),
                          (int)(w[i + 2] >> 16), (int)(w[i + 3] & 0xFFFF), (int)(w[i + 3] >> 16));
            }
            i += 4;
            continue;
        }
        if (cmd == 0xE1 && emit) {
            set_tpage((unsigned)(w[i] & 0x1FF));
        }
        i++;
    }
    return 0;
}

#define GPU_WALK_MAX 0x10000

static int gpu_last_steps;

/* Follows the chain from `ot`, decoding each 24-bit link with ps1_link_to_ptr.
 * Returns the number of primitives drawn. Without `emit`, nothing is drawn or written. */
static int walk(u_long *ot, int emit) {
    int steps;
    int drawn;
    u_long *packet;

    drawn = 0;
    packet = ot;
    for (steps = 0; packet != NULL && steps < GPU_WALK_MAX; steps++) {
        u_long tag;
        int len;

        tag = packet[0];
        len = (int)(tag >> 24);
        if (len != 0) {
            drawn += run_packet(packet + 1, len, emit);
        }
        packet = (u_long *)ps1_link_to_ptr((uint32_t)tag);
    }
    gpu_last_steps = steps;
    return drawn;
}

static int gpu_game_drew;
static int gpu_game_draw_calls;
static int gpu_game_packets;

void DrawOTag(u_long *ot) {
    gpu_game_drew = 1;
    gpu_game_draw_calls++;
    gpu_game_packets += walk(ot, 1);
    flush_verts();
}

void gpu_debug_game_stats(int *calls, int *packets) {
    *calls = gpu_game_draw_calls;
    *packets = gpu_game_packets;
}

int gpu_take_game_drew(void) {
    int drew;

    drew = gpu_game_drew;
    gpu_game_drew = 0;
    return drew;
}

int gpu_debug_walk(u_long *ot) {
    return walk(ot, 0);
}

int gpu_debug_last_steps(void) {
    return gpu_last_steps;
}

void gpu_clear_frame(void) {
    if (!gpu_ready) {
        return;
    }
    glClearColor(clear_rgb[0], clear_rgb[1], clear_rgb[2], 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
}

void gpu_clear_screen(u_char r, u_char g, u_char b) {
    flush_verts();
    clear_rgb[0] = (float)(r & 0xF8) / 248.0f;
    clear_rgb[1] = (float)(g & 0xF8) / 248.0f;
    clear_rgb[2] = (float)(b & 0xF8) / 248.0f;
    gpu_clear_frame();
}

void gpu_debug_reset_clear(void) {
    clear_rgb[0] = 13.0f / 255.0f;
    clear_rgb[1] = 18.0f / 255.0f;
    clear_rgb[2] = 31.0f / 255.0f;
}

void gpu_put_draw_env(u_short tpage, u_char isbg, u_char r, u_char g, u_char b) {
    set_tpage(tpage);
    if (isbg) {
        gpu_clear_screen(r, g, b);
    }
}

int gpu_debug_read_pixel(int x, int y, unsigned char rgb[3]) {
    GLint viewport[4];
    unsigned char rgba[4];
    int px;
    int py;

    flush_verts();
    glGetIntegerv(GL_VIEWPORT, viewport);
    px = (int)(((float)x + 0.5f) * (float)viewport[2] / 320.0f);
    py = (int)(((float)y + 0.5f) * (float)viewport[3] / 240.0f);
    glReadPixels(px, viewport[3] - 1 - py, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    memcpy(rgb, rgba, 3);
    return 0;
}

u_long *ClearOTag(u_long *ot, int n) {
    int i;

    for (i = 0; i < n - 1; i++) {
        ot[i] = ps1_ptr_to_link(&ot[i + 1]);
    }
    if (n > 0) {
        ot[n - 1] = PS1_LINK_END;
    }
    return ot;
}

u_long *ClearOTagR(u_long *ot, int n) {
    int i;

    for (i = n - 1; i > 0; i--) {
        ot[i] = ps1_ptr_to_link(&ot[i - 1]);
    }
    if (n > 0) {
        ot[0] = PS1_LINK_END;
    }
    return ot;
}

void gpu_draw_demo(void) {
    u_long quad_addr;

    memset(&demo_skip, 0, sizeof(demo_skip));
    memset(&demo_quad, 0, sizeof(demo_quad));
    demo_quad.code = 0x28;
    demo_quad.r0 = 32;
    demo_quad.g0 = 220;
    demo_quad.b0 = 48;
    demo_quad.x0 = 80;
    demo_quad.y0 = 60;
    demo_quad.x1 = 240;
    demo_quad.y1 = 60;
    demo_quad.x2 = 80;
    demo_quad.y2 = 180;
    demo_quad.x3 = 240;
    demo_quad.y3 = 180;
    demo_quad.tag = 5UL << 24;

    quad_addr = (u_long)(uintptr_t)&demo_quad & 0x00FFFFFFUL;
    demo_skip.code = 0x60;
    demo_skip.tag = (1UL << 24) | quad_addr;
    walk(&demo_skip.tag, 1);
    flush_verts();
}
