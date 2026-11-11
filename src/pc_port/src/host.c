#include <bg_trace.h>
#include <host.h>
#include <map_exporter.h>
#include <libgpu.h>
#include <object_trace.h>
#include <objects_dump.h>
#include <texture_dump.h>
#include <tmd_vram.h>

#include <SDL.h>
#include <GL/gl.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct SDL_Window *host_window;
jmp_buf host_exit_jmp;

static FILE *host_log;

static void log_to_file(void *userdata, int category, SDL_LogPriority priority, const char *message) {
    (void)userdata;
    (void)category;
    (void)priority;
    fprintf(host_log, "%s\n", message);
    fflush(host_log);
}

void host_log_open(const char *path) {
    FILE *list;

    host_log = fopen(path, "w");
    if (host_log != NULL) {
        SDL_LogSetOutputFunction(log_to_file, NULL);
    }
    list = fopen("f8-captures.txt", "w");
    if (list != NULL) {
        fclose(list);
    }
}

int host_tracing(void) {
    static int tracing = -1;
    const char *env;

    if (tracing < 0) {
        env = getenv("HMBTN_TRACE");
        tracing = env != NULL && strcmp(env, "1") == 0;
    }
    return tracing;
}

void host_quit(void) {
    longjmp(host_exit_jmp, 1);
}

void host_clear(void) {
    int w;
    int h;

    SDL_GL_GetDrawableSize(host_window, &w, &h);
    glViewport(0, 0, w, h);
    gpu_clear_frame();
}

static int f8_count;

/* One capture by default: every F8 overwrites f8-001.bmp and f8-captures.txt.
 * HMBTN_F8_LIST=1 keeps the numbered list. */
static int f8_keep_list(void) {
    const char *env;

    env = getenv("HMBTN_F8_LIST");
    return env != NULL && strcmp(env, "1") == 0;
}

int host_f8_index(void) {
    return f8_count;
}

static void note_f8(int f8_down) {
    static int was;
    FILE *list;
    int place;
    int scene;
    int saved;
    int rising;
    char line[160];

    rising = f8_down && !was;
    was = f8_down ? 1 : 0;
    if (!rising) {
        return;
    }
    f8_count = f8_keep_list() ? f8_count + 1 : 1;
    saved = gpu_save_f8(f8_count, &place, &scene);
    snprintf(line, sizeof(line), "f8 capture %d vblank %d place %d scene 0x%x file f8-%03d.bmp%s", f8_count,
             host_vblank_count(), place, scene, f8_count, saved == 0 ? "" : " save-failed");
    SDL_Log("%s", line);
    list = fopen("f8-captures.txt", f8_keep_list() ? "a" : "w");
    if (list != NULL) {
        fprintf(list, "%s\n", line);
        fclose(list);
    }
}

void host_present(void) {
    const unsigned char *keys;
    int f8;

    keys = SDL_GetKeyboardState(NULL);
    f8 = keys != NULL && keys[SDL_SCANCODE_F8] != 0;
    note_f8(f8);
    gpu_trace_frame();
    gpu_trace_camera();
    objects_dump_present(f8);
    object_trace_present(f8);
    tmd_vram_present(f8);
    texture_dump_present(f8);
    bg_trace_present(f8);
    SDL_GL_SwapWindow(host_window);
    map_export_tick();
    host_clear();
}
