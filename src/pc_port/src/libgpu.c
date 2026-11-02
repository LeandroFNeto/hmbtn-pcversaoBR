/* The host GPU: DrawOTag walks an ordering table, runs its transfers and draw-mode
 * words, and draws its polygons and rectangles with OpenGL, sampling the host VRAM
 * (vram.c) through one 1024 x 512 texture. */

#include <libgpu.h>
#include <object_trace.h>
#include <objects_dump.h>
#include <tmd_vram.h>
#include <ps1_memory.h>
#include <vram.h>
#include <farm_state.h>
#include <host.h>

#include <SDL.h>
#include <GL/gl.h>

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
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
typedef void(APIENTRY *PFN_glUniform1f)(GLint, GLfloat);
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
static PFN_glUniform1f p_glUniform1f;
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
static GpuVert *gpu_verts;
static int gpu_vert_count;
static int batch_subtract;

static float clear_rgb[3] = {13.0f / 255.0f, 18.0f / 255.0f, 31.0f / 255.0f};

/* The GPU draw mode, which persists across command lists as on the PS1. */
static int st_page_x;
static int st_page_y;
static int st_depth = 4;
static int st_abr;

/* Texture window in pixels (mask and offset). Mask 0 leaves the coordinate alone.
 * It persists across DrawOTag, matching the GPU register. */
static int tw_mask_x;
static int tw_mask_y;
static int tw_off_x;
static int tw_off_y;
static GLint loc_tw_mask_x = -1;
static GLint loc_tw_mask_y = -1;
static GLint loc_tw_off_x = -1;
static GLint loc_tw_off_y = -1;

/* The draw environment's clip, where this frame's pixels live in VRAM. */
static int clip_x;
static int clip_y;
static int clip_w = 320;
static int clip_h = 240;
static unsigned char refresh_rgba[2048 * 4];
static uint16_t refresh_words[320];

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
    "uniform float uTwMaskX;\n"
    "uniform float uTwMaskY;\n"
    "uniform float uTwOffX;\n"
    "uniform float uTwOffY;\n"
    "varying vec3 vColor;\n"
    "varying vec2 vUV;\n"
    "varying vec3 vPage;\n"
    "varying vec2 vClut;\n"
    "varying vec2 vMisc;\n"
    "float vram_word(vec2 p) {\n"
    "    vec4 t = texture2D(uVram, (mod(floor(p), vec2(1024.0, 512.0)) + 0.5) / vec2(1024.0, 512.0));\n"
    "    return floor(t.r * 255.0 + 0.5) + floor(t.a * 255.0 + 0.5) * 256.0;\n"
    "}\n"
    "float tw_bit(float value, float bit) {\n"
    "    return mod(floor(value / bit), 2.0);\n"
    "}\n"
    "float tw_axis(float c, float mask, float off) {\n"
    "    c = mod(floor(c), 256.0);\n"
    "    c = c - tw_bit(c, 8.0) * 8.0 * tw_bit(mask, 8.0) + tw_bit(off, 8.0) * 8.0 * tw_bit(mask, 8.0);\n"
    "    c = c - tw_bit(c, 16.0) * 16.0 * tw_bit(mask, 16.0) + tw_bit(off, 16.0) * 16.0 * tw_bit(mask, 16.0);\n"
    "    c = c - tw_bit(c, 32.0) * 32.0 * tw_bit(mask, 32.0) + tw_bit(off, 32.0) * 32.0 * tw_bit(mask, 32.0);\n"
    "    c = c - tw_bit(c, 64.0) * 64.0 * tw_bit(mask, 64.0) + tw_bit(off, 64.0) * 64.0 * tw_bit(mask, 64.0);\n"
    "    c = c - tw_bit(c, 128.0) * 128.0 * tw_bit(mask, 128.0) + tw_bit(off, 128.0) * 128.0 * tw_bit(mask, 128.0);\n"
    "    return c;\n"
    "}\n"
    "void main() {\n"
    "    vec3 rgb = vColor;\n"
    "    float stp = 1.0;\n"
    "    if (vPage.z > 0.5) {\n"
    "        vec2 uv = vec2(tw_axis(vUV.x, uTwMaskX, uTwOffX), tw_axis(vUV.y, uTwMaskY, uTwOffY));\n"
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

    if (gpu_verts == NULL) {
        gpu_verts = (GpuVert *)malloc((size_t)GPU_VERT_MAX * sizeof(GpuVert));
        if (gpu_verts == NULL) {
            SDL_Log("gpu vertex buffer allocation failed");
            return 1;
        }
    }
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
    p_glUniform1f = (PFN_glUniform1f)load_proc("glUniform1f", &missing);
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
    loc_tw_mask_x = p_glGetUniformLocation(gpu_program, "uTwMaskX");
    loc_tw_mask_y = p_glGetUniformLocation(gpu_program, "uTwMaskY");
    loc_tw_off_x = p_glGetUniformLocation(gpu_program, "uTwOffX");
    loc_tw_off_y = p_glGetUniformLocation(gpu_program, "uTwOffY");
    if (loc_tw_mask_x < 0 || loc_tw_mask_y < 0 || loc_tw_off_x < 0 || loc_tw_off_y < 0) {
        SDL_Log("shader texture-window uniforms missing");
        return -1;
    }
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
    p_glUniform1f(loc_tw_mask_x, (float)tw_mask_x);
    p_glUniform1f(loc_tw_mask_y, (float)tw_mask_y);
    p_glUniform1f(loc_tw_off_x, (float)tw_off_x);
    p_glUniform1f(loc_tw_off_y, (float)tw_off_y);
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

/* One drawn primitive, kept only while HMBTN_TRACE=1 so a presented frame can say
 * which packet covers a black pixel. Static, and never touched when tracing is off. */
typedef struct TracePrim {
    unsigned cmd;
    short x[4];
    short y[4];
    unsigned char u[4];
    unsigned char v[4];
    unsigned char n;
    unsigned char depth;
    unsigned char r, g, b;
    short page_x, page_y;
    short clut_x, clut_y;
    short tw_mask_x, tw_mask_y, tw_off_x, tw_off_y;
} TracePrim;

#define TRACE_PRIM_MAX 1536
static TracePrim *trace_prims;
static int trace_prim_count;
static int trace_prim_overflow;
static unsigned trace_e2_word;
static int trace_e2_count;
static int trace_line_share;
static unsigned trace_line_cmd;
static unsigned trace_line_next;
static int trace_line_words_seen;
static int trace_line_avail;
static int trace_fill_share;
static unsigned trace_fill_next;
static unsigned char trace_grid[60][80];
static unsigned char trace_seen[60][80];
static int *trace_queue;

extern int main_scene;

static int trace_line_words(unsigned cmd) {
    switch (cmd & ~0x02u) {
    case 0x40:
        return 3;
    case 0x50:
        return 4;
    case 0x48:
        return 5;
    case 0x4C:
        return 6;
    default:
        return 3;
    }
}

static void trace_note_prim(unsigned cmd, const Prim *p) {
    TracePrim *t;
    int i;

    if (trace_prims == NULL) {
        trace_prims = (TracePrim *)malloc((size_t)TRACE_PRIM_MAX * sizeof(TracePrim));
        if (trace_prims == NULL) {
            return;
        }
    }
    if (trace_prim_count >= TRACE_PRIM_MAX) {
        trace_prim_overflow = 1;
        return;
    }
    t = &trace_prims[trace_prim_count++];
    t->cmd = cmd;
    t->n = (unsigned char)p->count;
    t->depth = (unsigned char)p->depth;
    t->r = (unsigned char)(p->v[0].r * 255.0f);
    t->g = (unsigned char)(p->v[0].g * 255.0f);
    t->b = (unsigned char)(p->v[0].b * 255.0f);
    t->page_x = (short)p->page_x;
    t->page_y = (short)p->page_y;
    t->clut_x = (short)p->clut_x;
    t->clut_y = (short)p->clut_y;
    t->tw_mask_x = (short)tw_mask_x;
    t->tw_mask_y = (short)tw_mask_y;
    t->tw_off_x = (short)tw_off_x;
    t->tw_off_y = (short)tw_off_y;
    for (i = 0; i < 4; i++) {
        t->x[i] = 0;
        t->y[i] = 0;
        t->u[i] = 0;
        t->v[i] = 0;
    }
    for (i = 0; i < p->count && i < 4; i++) {
        t->x[i] = (short)p->v[i].x;
        t->y[i] = (short)p->v[i].y;
        t->u[i] = (unsigned char)p->v[i].u;
        t->v[i] = (unsigned char)p->v[i].v;
    }
}

static void trace_note_line(unsigned cmd, int avail, unsigned next) {
    int words;

    words = trace_line_words(cmd);
    if (avail > words) {
        trace_line_share = 1;
        trace_line_cmd = cmd;
        trace_line_next = next;
        trace_line_words_seen = words;
        trace_line_avail = avail;
    }
}

static void trace_note_fill(int avail, unsigned next) {
    if (avail > 3) {
        trace_fill_share = 1;
        trace_fill_next = next;
    }
}

static void trace_note_e2(u_long word) {
    if ((word & 0x000FFFFFu) != 0) {
        trace_e2_word = (unsigned)word;
        trace_e2_count++;
    }
}

static void object_note_prim(unsigned cmd, const Prim *p);

static void objects_from_prim(unsigned cmd, const Prim *p) {
    short xy[8];
    int i;

    if (!objects_dump_collecting() && !tmd_vram_collecting()) {
        return;
    }
    for (i = 0; i < 8; i++) {
        xy[i] = 0;
    }
    for (i = 0; i < p->count && i < 4; i++) {
        xy[i * 2] = (short)p->v[i].x;
        xy[i * 2 + 1] = (short)p->v[i].y;
    }
    if (objects_dump_collecting()) {
        objects_dump_note_drawn(cmd, p->count, xy, (int)(p->v[0].r * 255.0f + 0.5f), (int)(p->v[0].g * 255.0f + 0.5f),
                                (int)(p->v[0].b * 255.0f + 0.5f), p->depth > 0.0f, (int)p->v[0].u, (int)p->v[0].v,
                                (int)p->page_x, (int)p->page_y, (int)p->clut_x, (int)p->clut_y, (int)p->depth);
    }
    if (tmd_vram_collecting()) {
        tmd_vram_note_drawn(cmd, p->count, xy, p->depth > 0.0f, (int)p->v[0].u, (int)p->v[0].v, (int)p->page_x,
                            (int)p->page_y, (int)p->clut_x, (int)p->clut_y, (int)p->depth);
    }
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
    if (host_tracing()) {
        trace_note_prim(cmd, &p);
    }
    object_note_prim(cmd, &p);
    objects_from_prim(cmd, &p);
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
    if (host_tracing()) {
        trace_note_prim(cmd, &p);
    }
    object_note_prim(cmd, &p);
    objects_from_prim(cmd, &p);
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
            int drew;

            drew = decode_poly(w + i, len - i, emit);
            if (emit && drew) {
                object_trace_count_poly();
            }
            return drew;
        }
        if (cmd >= 0x60 && cmd <= 0x7F) {
            int drew;

            drew = decode_rect(w + i, len - i, emit);
            if (emit && drew) {
                object_trace_count_rect();
            }
            return drew;
        }
        if (cmd >= 0x40 && cmd <= 0x5F) {
            if (emit && host_tracing()) {
                trace_note_line(cmd, len - i, i + trace_line_words(cmd) < len ? (unsigned)(w[i + trace_line_words(cmd)] >> 24) : 0);
            }
            if (emit) {
                int words;
                int next;

                words = trace_line_words(cmd);
                next = i + words < len ? (int)(w[i + words] >> 24) : -1;
                object_trace_count_line(len - i > words);
                objects_dump_note_skip(cmd, next);
            }
            return 0;
        }
        if (emit) {
            object_trace_count_other(cmd == 0xE6);
        }
        if (cmd == 0x02 && emit && host_tracing()) {
            trace_note_fill(len - i, i + 3 < len ? (unsigned)(w[i + 3] >> 24) : 0);
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
        if (cmd == 0xE2 && emit) {
            int mx;
            int my;
            int ox;
            int oy;

            mx = (int)((w[i] & 0x1F) << 3);
            my = (int)(((w[i] >> 5) & 0x1F) << 3);
            ox = (int)(((w[i] >> 10) & 0x1F) << 3);
            oy = (int)(((w[i] >> 15) & 0x1F) << 3);
            if (mx != tw_mask_x || my != tw_mask_y || ox != tw_off_x || oy != tw_off_y) {
                flush_verts();
                tw_mask_x = mx;
                tw_mask_y = my;
                tw_off_x = ox;
                tw_off_y = oy;
            }
            if (host_tracing()) {
                trace_note_e2(w[i]);
            }
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

void gpu_put_draw_env(u_short tpage, u_char isbg, u_char r, u_char g, u_char b, int x, int y, int w, int h) {
    set_tpage(tpage);
    clip_x = x;
    clip_y = y;
    clip_w = w;
    clip_h = h;
    if (isbg) {
        gpu_clear_screen(r, g, b);
    }
}

int gpu_debug_vert_bytes(void) {
    return (int)sizeof(GpuVert);
}

int gpu_debug_vert_cap(void) {
    return GPU_VERT_MAX;
}

/* Copies the part of (x, y, w, h) that overlaps the draw buffer from the back buffer
 * into the static VRAM. One GL row per VRAM row, into static buffers. */
void gpu_refresh_draw_overlap(int x, int y, int w, int h) {
    int x0;
    int y0;
    int x1;
    int y1;
    int row;
    int n;
    int i;
    GLint viewport[4];

    if (!gpu_ready || w <= 0 || h <= 0 || clip_w <= 0 || clip_h <= 0) {
        return;
    }
    x0 = x > clip_x ? x : clip_x;
    y0 = y > clip_y ? y : clip_y;
    x1 = x + w < clip_x + clip_w ? x + w : clip_x + clip_w;
    y1 = y + h < clip_y + clip_h ? y + h : clip_y + clip_h;
    if (x0 >= x1 || y0 >= y1) {
        return;
    }
    flush_verts();
    glGetIntegerv(GL_VIEWPORT, viewport);
    n = x1 - x0;
    if (n > 320) {
        n = 320;
    }
    for (row = y0; row < y1; row++) {
        int sy;
        int py;
        int gy;
        int gx0;
        int gx1;
        int width;

        sy = row - clip_y;
        py = (int)(((float)sy + 0.5f) * (float)viewport[3] / 240.0f);
        gy = viewport[3] - 1 - py;
        gx0 = (int)(((float)(x0 - clip_x) + 0.5f) * (float)viewport[2] / 320.0f);
        gx1 = (int)(((float)(x0 - clip_x + n - 1) + 0.5f) * (float)viewport[2] / 320.0f);
        width = gx1 - gx0 + 1;
        if (width < 1) {
            width = 1;
        }
        if (width > 2048) {
            width = 2048;
        }
        glReadPixels(gx0, gy, width, 1, GL_RGBA, GL_UNSIGNED_BYTE, refresh_rgba);
        for (i = 0; i < n; i++) {
            int gx;
            unsigned char *p;

            gx = (int)(((float)(x0 - clip_x + i) + 0.5f) * (float)viewport[2] / 320.0f) - gx0;
            if (gx < 0) {
                gx = 0;
            }
            if (gx >= width) {
                gx = width - 1;
            }
            p = refresh_rgba + gx * 4;
            refresh_words[i] = (uint16_t)((p[0] >> 3) | ((p[1] >> 3) << 5) | ((p[2] >> 3) << 10));
        }
        vram_load(x0, row, n, 1, refresh_words);
    }
}

static float trace_cross(float ax, float ay, float bx, float by, float cx, float cy) {
    return (bx - ax) * (cy - ay) - (by - ay) * (cx - ax);
}

static int trace_inside(float px, float py, float ax, float ay, float bx, float by, float cx, float cy) {
    float c1;
    float c2;
    float c3;

    c1 = trace_cross(ax, ay, bx, by, px, py);
    c2 = trace_cross(bx, by, cx, cy, px, py);
    c3 = trace_cross(cx, cy, ax, ay, px, py);
    if (c1 >= -0.5f && c2 >= -0.5f && c3 >= -0.5f) {
        return 1;
    }
    if (c1 <= 0.5f && c2 <= 0.5f && c3 <= 0.5f) {
        return 1;
    }
    return 0;
}

static int trace_tw(int c, int mask, int off) {
    static const int bits[5] = {8, 16, 32, 64, 128};
    int i;

    c &= 255;
    for (i = 0; i < 5; i++) {
        if (mask & bits[i]) {
            c = (c & ~bits[i]) | (off & bits[i]);
        }
    }
    return c;
}

/* Last primitive whose triangle contains (px, py). Later packets overdraw earlier ones. */
static int trace_cover(float px, float py, int *i0, int *i1, int *i2) {
    int i;
    int hit;

    hit = -1;
    for (i = 0; i < trace_prim_count; i++) {
        TracePrim *p;

        p = &trace_prims[i];
        if (p->n >= 3 && trace_inside(px, py, p->x[0], p->y[0], p->x[1], p->y[1], p->x[2], p->y[2])) {
            hit = i;
            *i0 = 0;
            *i1 = 1;
            *i2 = 2;
        } else if (p->n == 4 && trace_inside(px, py, p->x[1], p->y[1], p->x[3], p->y[3], p->x[2], p->y[2])) {
            hit = i;
            *i0 = 1;
            *i1 = 3;
            *i2 = 2;
        }
    }
    return hit;
}

static unsigned trace_word(const TracePrim *p, float px, float py, int i0, int i1, int i2, int *u_out, int *v_out) {
    float ax;
    float ay;
    float bx;
    float by;
    float cx;
    float cy;
    float area;
    float w0;
    float w1;
    float w2;
    float u;
    float v;
    int tu;
    int tv;
    int x;
    int y;
    int index;
    unsigned word;

    ax = (float)p->x[i0];
    ay = (float)p->y[i0];
    bx = (float)p->x[i1];
    by = (float)p->y[i1];
    cx = (float)p->x[i2];
    cy = (float)p->y[i2];
    area = trace_cross(ax, ay, bx, by, cx, cy);
    if (area > -0.5f && area < 0.5f) {
        u = (float)p->u[i0];
        v = (float)p->v[i0];
    } else {
        w0 = trace_cross(bx, by, cx, cy, px, py) / area;
        w1 = trace_cross(cx, cy, ax, ay, px, py) / area;
        w2 = trace_cross(ax, ay, bx, by, px, py) / area;
        u = w0 * (float)p->u[i0] + w1 * (float)p->u[i1] + w2 * (float)p->u[i2];
        v = w0 * (float)p->v[i0] + w1 * (float)p->v[i1] + w2 * (float)p->v[i2];
    }
    if (u < 0.0f) {
        u = 0.0f;
    }
    if (v < 0.0f) {
        v = 0.0f;
    }
    tu = trace_tw((int)u, p->tw_mask_x, p->tw_off_x);
    tv = trace_tw((int)v, p->tw_mask_y, p->tw_off_y);
    *u_out = tu;
    *v_out = tv;
    if (p->depth == 0) {
        return 0;
    }
    if (p->depth <= 4) {
        x = p->page_x + tu / 4;
        y = p->page_y + tv;
        word = vram[y & 511][x & 1023];
        index = (int)((word >> ((tu & 3) * 4)) & 0xF);
        return vram[p->clut_y & 511][(p->clut_x + index) & 1023];
    }
    if (p->depth <= 8) {
        x = p->page_x + tu / 2;
        y = p->page_y + tv;
        word = vram[y & 511][x & 1023];
        index = (tu & 1) ? (int)((word >> 8) & 0xFF) : (int)(word & 0xFF);
        return vram[p->clut_y & 511][(p->clut_x + index) & 1023];
    }
    x = p->page_x + tu;
    y = p->page_y + tv;
    return vram[y & 511][x & 1023];
}

static void object_note_prim(unsigned cmd, const Prim *p) {
    short xy[8];
    TracePrim sample;
    int i;
    int covers;
    int i0;
    int i1;
    int i2;
    int tu;
    int tv;
    unsigned word;

    if (!object_trace_collecting()) {
        return;
    }
    for (i = 0; i < 4; i++) {
        xy[i * 2] = 0;
        xy[i * 2 + 1] = 0;
    }
    for (i = 0; i < p->count && i < 4; i++) {
        xy[i * 2] = (short)p->v[i].x;
        xy[i * 2 + 1] = (short)p->v[i].y;
    }
    covers = 0;
    i0 = 0;
    i1 = 1;
    i2 = 2;
    if (p->count >= 3 &&
        trace_inside(160.0f, 120.0f, p->v[0].x, p->v[0].y, p->v[1].x, p->v[1].y, p->v[2].x, p->v[2].y)) {
        covers = 1;
    } else if (p->count == 4 &&
               trace_inside(160.0f, 120.0f, p->v[1].x, p->v[1].y, p->v[3].x, p->v[3].y, p->v[2].x, p->v[2].y)) {
        covers = 1;
        i0 = 1;
        i1 = 3;
        i2 = 2;
    }
    word = 0;
    if (covers && p->depth > 0.0f) {
        memset(&sample, 0, sizeof(sample));
        sample.n = (unsigned char)p->count;
        sample.depth = (unsigned char)p->depth;
        sample.page_x = (short)p->page_x;
        sample.page_y = (short)p->page_y;
        sample.clut_x = (short)p->clut_x;
        sample.clut_y = (short)p->clut_y;
        sample.tw_mask_x = (short)tw_mask_x;
        sample.tw_mask_y = (short)tw_mask_y;
        sample.tw_off_x = (short)tw_off_x;
        sample.tw_off_y = (short)tw_off_y;
        for (i = 0; i < p->count && i < 4; i++) {
            sample.x[i] = xy[i * 2];
            sample.y[i] = xy[i * 2 + 1];
            sample.u[i] = (unsigned char)p->v[i].u;
            sample.v[i] = (unsigned char)p->v[i].v;
        }
        word = trace_word(&sample, 160.0f, 120.0f, i0, i1, i2, &tu, &tv);
    }
    object_trace_note_drawn(cmd, p->count, xy, (int)(p->v[0].r * 255.0f + 0.5f), (int)(p->v[0].g * 255.0f + 0.5f),
                            (int)(p->v[0].b * 255.0f + 0.5f), p->depth > 0.0f, word, covers);
}

static int trace_on_farm(int scene) {
    /* SCENE_FARM, the minigame return, and the load/save/menu/staff returns. */
    return scene == 2 || scene == 6 || scene == 14 || scene == 15 || scene == 19 || scene == 20;
}

static const char *trace_region(int location, int scene, int pixels, int cy, int nonblack) {
    /* A pure fade is black with nothing drawn on it. The flashback keeps a sprite or a line of text. */
    if (pixels > (320 * 240) / 2 && nonblack >= 4) {
        return "flashback background";
    }
    if (scene >= AREA_HOUSE0_SPRING && scene <= AREA_HOUSE2_WINTER && cy >= 80) {
        return "the bed";
    }
    if (location == PLACE_FARM) {
        return "left of the house";
    }
    return "black center";
}

/* Writes the presented frame one row at a time. The row buffer is the one StoreImage
 * already keeps; this runs only under HMBTN_TRACE and only the first time each region is seen. */
static void trace_save_view(const char *region, int ox, int oy, int vw, int vh) {
    static int saved_house;
    static int saved_bed;
    static int saved_flash;
    const char *path;
    FILE *out;
    unsigned char header[54];
    uint32_t size;
    int y;
    int i;

    path = NULL;
    if (strcmp(region, "left of the house") == 0 && !saved_house) {
        path = "gpu_left_of_the_house.bmp";
        saved_house = 1;
    } else if (strcmp(region, "the bed") == 0 && !saved_bed) {
        path = "gpu_the_bed.bmp";
        saved_bed = 1;
    } else if (strcmp(region, "flashback background") == 0 && !saved_flash) {
        path = "gpu_flashback_background.bmp";
        saved_flash = 1;
    }
    if (path == NULL || vw > 2048) {
        return;
    }
    out = fopen(path, "wb");
    if (out == NULL) {
        SDL_Log("trace gpu: could not save %s", path);
        return;
    }
    memset(header, 0, sizeof(header));
    header[0] = 'B';
    header[1] = 'M';
    size = 54u + (uint32_t)vw * (uint32_t)vh * 4u;
    memcpy(header + 2, &size, 4);
    header[10] = 54;
    header[14] = 40;
    memcpy(header + 18, &vw, 4);
    memcpy(header + 22, &vh, 4);
    header[26] = 1;
    header[28] = 32;
    fwrite(header, 1, sizeof(header), out);
    for (y = 0; y < vh; y++) {
        glReadPixels(ox, oy + y, vw, 1, GL_RGBA, GL_UNSIGNED_BYTE, refresh_rgba);
        for (i = 0; i < vw; i++) {
            unsigned char swap;

            swap = refresh_rgba[i * 4];
            refresh_rgba[i * 4] = refresh_rgba[i * 4 + 2];
            refresh_rgba[i * 4 + 2] = swap;
        }
        fwrite(refresh_rgba, 4, (size_t)vw, out);
    }
    fclose(out);
    SDL_Log("trace gpu: saved %s", path);
}

int gpu_fill_black_grid(unsigned char grid[60][80]) {
    GLint viewport[4];
    int x;
    int y;
    int vw;
    int vh;

    if (!gpu_ready) {
        return 0;
    }
    glGetIntegerv(GL_VIEWPORT, viewport);
    vw = viewport[2];
    vh = viewport[3];
    if (vw <= 0 || vh <= 0 || vw > 2048) {
        return 0;
    }
    memset(grid, 0, 60 * 80);
    for (y = 0; y < 60; y++) {
        int py;
        int gy;

        py = (int)(((float)(y * 4 + 2) + 0.5f) * (float)vh / 240.0f);
        gy = vh - 1 - py;
        if (gy < 0) {
            gy = 0;
        }
        if (gy >= vh) {
            gy = vh - 1;
        }
        glReadPixels(viewport[0], viewport[1] + gy, vw, 1, GL_RGBA, GL_UNSIGNED_BYTE, refresh_rgba);
        for (x = 0; x < 80; x++) {
            int px;
            unsigned char *pix;

            px = (int)(((float)(x * 4 + 2) + 0.5f) * (float)vw / 320.0f);
            if (px < 0) {
                px = 0;
            }
            if (px >= vw) {
                px = vw - 1;
            }
            pix = refresh_rgba + (size_t)px * 4u;
            if (pix[0] < 8 && pix[1] < 8 && pix[2] < 8) {
                grid[y][x] = 1;
            }
        }
    }
    return 1;
}

/* Reads one presented frame while tracing and logs decision 4: a non-zero texture
 * window, whether each black center is covered, and a line or fill that shares its packet. */
void gpu_trace_frame(void) {
    GLint viewport[4];
    int location;
    int scene;
    int vw;
    int vh;
    int x;
    int y;
    int blobs;
    int nonblack;
    int b;
    int i0;
    int i1;
    int i2;
    struct {
        int n;
        int sx;
        int sy;
    } found[8];

    if (!host_tracing() || !gpu_ready) {
        return;
    }
    if (!trace_on_farm(main_scene) || (host_vblank_count() % 60) != 0) {
        trace_prim_count = 0;
        trace_prim_overflow = 0;
        return;
    }
    location = 0;
    scene = 0;
    if (farm_area.mode.data == &farm_area.mode.script) {
        location = farm_area.mode.script.head.place.location;
        scene = farm_area.mode.script.head.scene;
    }
    flush_verts();
    glGetIntegerv(GL_VIEWPORT, viewport);
    vw = viewport[2];
    vh = viewport[3];
    if (vw <= 0 || vh <= 0 || vw > 2048 || vh > 1080) {
        trace_prim_count = 0;
        trace_prim_overflow = 0;
        return;
    }
    memset(trace_grid, 0, sizeof(trace_grid));
    memset(trace_seen, 0, sizeof(trace_seen));
    nonblack = 0;
    for (y = 0; y < 60; y++) {
        int py;
        int gy;

        py = (int)(((float)(y * 4 + 2) + 0.5f) * (float)vh / 240.0f);
        gy = vh - 1 - py;
        if (gy < 0) {
            gy = 0;
        }
        if (gy >= vh) {
            gy = vh - 1;
        }
        glReadPixels(viewport[0], viewport[1] + gy, vw, 1, GL_RGBA, GL_UNSIGNED_BYTE, refresh_rgba);
        for (x = 0; x < 80; x++) {
            int px;
            unsigned char *pix;

            px = (int)(((float)(x * 4 + 2) + 0.5f) * (float)vw / 320.0f);
            if (px < 0) {
                px = 0;
            }
            if (px >= vw) {
                px = vw - 1;
            }
            pix = refresh_rgba + (size_t)px * 4u;
            if (pix[0] < 8 && pix[1] < 8 && pix[2] < 8) {
                trace_grid[y][x] = 1;
            } else {
                nonblack++;
            }
        }
    }
    blobs = 0;
    if (trace_queue == NULL) {
        trace_queue = (int *)malloc(60 * 80 * sizeof(int));
    }
    if (trace_queue != NULL) {
    for (y = 0; y < 60; y++) {
        for (x = 0; x < 80; x++) {
            int *queue;
            int qh;
            int qt;
            int n;
            int sx;
            int sy;
            int k;

            queue = trace_queue;
            if (!trace_grid[y][x] || trace_seen[y][x]) {
                continue;
            }
            qh = 0;
            qt = 0;
            n = 0;
            sx = 0;
            sy = 0;
            queue[qt++] = y * 80 + x;
            trace_seen[y][x] = 1;
            while (qh < qt) {
                int cur;
                int cx;
                int cy;
                static const int dx[4] = {-1, 1, 0, 0};
                static const int dy[4] = {0, 0, -1, 1};

                cur = queue[qh++];
                cx = cur % 80;
                cy = cur / 80;
                n++;
                sx += cx;
                sy += cy;
                for (k = 0; k < 4; k++) {
                    int nx;
                    int ny;

                    nx = cx + dx[k];
                    ny = cy + dy[k];
                    if (nx < 0 || ny < 0 || nx >= 80 || ny >= 60) {
                        continue;
                    }
                    if (trace_grid[ny][nx] && !trace_seen[ny][nx]) {
                        trace_seen[ny][nx] = 1;
                        queue[qt++] = ny * 80 + nx;
                    }
                }
            }
            if (n < 30) {
                continue;
            }
            if (blobs < 8) {
                found[blobs].n = n;
                found[blobs].sx = sx;
                found[blobs].sy = sy;
                blobs++;
            } else {
                int small;

                small = 0;
                for (k = 1; k < 8; k++) {
                    if (found[k].n < found[small].n) {
                        small = k;
                    }
                }
                if (n > found[small].n) {
                    found[small].n = n;
                    found[small].sx = sx;
                    found[small].sy = sy;
                }
            }
        }
    }
    }
    SDL_Log("trace gpu: frame vblank %d place %d scene 0x%x prims %d overflow %d black %d e2 %d word 0x%08x line %d cmd 0x%02x next 0x%02x fill %d next 0x%02x",
            host_vblank_count(), location, scene, trace_prim_count, trace_prim_overflow, blobs, trace_e2_count,
            trace_e2_word, trace_line_share, trace_line_cmd, trace_line_next, trace_fill_share, trace_fill_next);
    for (b = 0; b < blobs; b++) {
        int cx;
        int cy;
        int pixels;
        int hit;
        int tu;
        int tv;
        unsigned word;
        const char *region;
        TracePrim *p;

        cx = found[b].n > 0 ? (found[b].sx / found[b].n) * 4 + 2 : 0;
        cy = found[b].n > 0 ? (found[b].sy / found[b].n) * 4 + 2 : 0;
        pixels = found[b].n * 16;
        region = trace_region(location, scene, pixels, cy, nonblack);
        hit = trace_cover((float)cx, (float)cy, &i0, &i1, &i2);
        tu = 0;
        tv = 0;
        word = 0;
        p = NULL;
        if (hit >= 0) {
            p = &trace_prims[hit];
            if (p->depth > 0) {
                word = trace_word(p, (float)cx, (float)cy, i0, i1, i2, &tu, &tv);
            }
        }
        if (p != NULL && p->depth > 0) {
            SDL_Log("trace gpu: %s center (%d,%d) pixels %d covered 1 cmd 0x%02x page %d,%d depth %d clut %d,%d uv %d,%d word 0x%04x win %d,%d,%d,%d",
                    region, cx, cy, pixels, p->cmd, p->page_x, p->page_y, p->depth, p->clut_x, p->clut_y, tu, tv, word,
                    p->tw_mask_x, p->tw_mask_y, p->tw_off_x, p->tw_off_y);
        } else if (p != NULL) {
            SDL_Log("trace gpu: %s center (%d,%d) pixels %d covered 1 cmd 0x%02x untextured rgb %d,%d,%d",
                    region, cx, cy, pixels, p->cmd, p->r, p->g, p->b);
        } else {
            SDL_Log("trace gpu: %s center (%d,%d) pixels %d covered 0", region, cx, cy, pixels);
        }
        trace_save_view(region, viewport[0], viewport[1], vw, vh);
    }
    trace_prim_count = 0;
    trace_prim_overflow = 0;
    trace_e2_count = 0;
    trace_e2_word = 0;
    trace_line_share = 0;
    trace_line_cmd = 0;
    trace_line_next = 0;
    trace_fill_share = 0;
    trace_fill_next = 0;
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
