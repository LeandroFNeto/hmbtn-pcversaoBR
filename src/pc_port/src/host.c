#include <host.h>
#include <libgpu.h>
#include <object_trace.h>
#include <objects_dump.h>
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
    host_log = fopen(path, "w");
    if (host_log != NULL) {
        SDL_LogSetOutputFunction(log_to_file, NULL);
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

void host_present(void) {
    const unsigned char *keys;
    int f8;

    keys = SDL_GetKeyboardState(NULL);
    f8 = keys != NULL && keys[SDL_SCANCODE_F8] != 0;
    gpu_trace_frame();
    objects_dump_present(f8);
    object_trace_present(f8);
    tmd_vram_present(f8);
    SDL_GL_SwapWindow(host_window);
    host_clear();
}
