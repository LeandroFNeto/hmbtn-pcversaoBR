/* PC entry point. The PlayStation main is compiled as ps1_main (-Dmain=ps1_main).
 * After the window and the PS1 address space are ready, the game runs on this thread;
 * the host runs only inside its VSync calls (libetc.c). Closing the window jumps back
 * here through host_exit_jmp. */

#ifdef main
#undef main
#endif

#define SDL_MAIN_HANDLED
#include <SDL.h>

#include <cd_drive.h>
#include <gte.h>
#include <host.h>
#include <host_input.h>
#include <libetc.h>
#include <libgpu.h>
#include <memcard.h>
#include <ps1_memory.h>
#include <spu_core.h>

#include <stdlib.h>
#include <string.h>

extern int ps1_main(void);

static SDL_GLContext host_context;

int cd_drive_self_check(void);
int vram_self_check(void);
int input_self_check(void);
int audio_self_check(void);
int seq_self_check(void);
int memcard_self_check(void);
int farm_tables_self_check(void);
int gte_library_self_check(void);
int object_trace_self_check(void);
int objects_dump_self_check(void);
int tmd_vram_self_check(void);
int item_art_self_check(void);
int item_picture_self_check(void);
int bin_sample_self_check(void);
int mods_self_check(void);
int model_tex_self_check(void);
int texture_dump_self_check(void);
int scenery_self_check(void);
int bg_trace_self_check(void);
int album_self_check(void);
int texture_cache_self_check(void);
int audio_ringbuffer_self_check(void);
int force_props_self_check(void);
int model_art_self_check(void);
int map_exporter_self_check(void);
int event_guard_self_check(void);
void map_export_args(int argc, char **argv);

/* The game spins in CdSearchFile if the archive is missing, so check it first. */
static int open_disc(void) {
    static const char *const required[] = {"\\A_FILE.HDT;1", "\\A_FILE.BIN;1"};
    uint32_t lba;
    uint32_t size;
    int missing;
    int i;

    cd_drive_open(NULL);
    missing = 0;
    for (i = 0; i < 2; i++) {
        if (cd_drive_find(required[i], &lba, &size) != 0) {
            SDL_Log("disc folder %s has no %s", cd_drive_dir(), required[i] + 1);
            missing = 1;
        }
    }
    return missing ? -1 : 0;
}

static void teardown(void) {
    memcard_close();
    gpu_gl_shutdown();
    host_input_shutdown();
    spu_audio_close();
    if (host_context != NULL) {
        SDL_GL_DeleteContext(host_context);
    }
    if (host_window != NULL) {
        SDL_DestroyWindow(host_window);
    }
    SDL_Quit();
}

int main(int argc, char *argv[]) {
    const char *selftest;

    map_export_args(argc, argv);

    host_log_open("hmbtn-host.log");
    host_install_crash_logger();
    if (gte_startup_check() != 0) {
        SDL_Log("GTE startup check failed");
        return 1;
    }

    SDL_SetMainReady();
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_GAMECONTROLLER) != 0) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }
    host_input_init();
    spu_audio_open();
    if (ps1_memory_map() != 0 || ps1_memory_self_check() != 0) {
        SDL_Quit();
        return 1;
    }
#if HMBTN_RUN_GAME
    if (open_disc() != 0) {
        SDL_Quit();
        return 1;
    }
#endif

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);

    host_window = SDL_CreateWindow("Harvest Moon: Back to Nature (PC Port)",
                                   SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                   640, 480, SDL_WINDOW_OPENGL);
    if (host_window == NULL) {
        SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
        teardown();
        return 1;
    }
    host_context = SDL_GL_CreateContext(host_window);
    if (host_context == NULL) {
        SDL_Log("SDL_GL_CreateContext failed: %s", SDL_GetError());
        teardown();
        return 1;
    }
    if (gpu_gl_init() != 0) {
        teardown();
        return 1;
    }
    host_clear();

    if (setjmp(host_exit_jmp) == 0) {
        selftest = getenv("HMBTN_SELFTEST");
        if (selftest != NULL && strcmp(selftest, "1") == 0) {
            cd_drive_open(NULL);
            if (host_frame_self_check() != 0 || vram_self_check() != 0 || input_self_check() != 0 ||
                audio_self_check() != 0 || seq_self_check() != 0 || cd_drive_self_check() != 0 ||
                memcard_self_check() != 0 || farm_tables_self_check() != 0 || gte_library_self_check() != 0 ||
                objects_dump_self_check() != 0 || object_trace_self_check() != 0 || tmd_vram_self_check() != 0 ||
                item_art_self_check() != 0 || item_picture_self_check() != 0 ||
                bin_sample_self_check() != 0 || mods_self_check() != 0 || model_tex_self_check() != 0 ||
                texture_dump_self_check() != 0 ||
                scenery_self_check() != 0 || bg_trace_self_check() != 0 || album_self_check() != 0 ||
                texture_cache_self_check() != 0 ||
                audio_ringbuffer_self_check() != 0 || force_props_self_check() != 0 || model_art_self_check() != 0 ||
                map_exporter_self_check() != 0 || event_guard_self_check() != 0) {
                teardown();
                return 1;
            }
        }
#if HMBTN_RUN_GAME
        host_start_watchdog(host_vblank_counter());
        ps1_main();
#else
        for (;;) {
            VSync(0);
        }
#endif
    }

    {
        int calls;
        int packets;

        gpu_debug_game_stats(&calls, &packets);
        SDL_Log("host exit after %d vblanks, %d game DrawOTag calls, %d drawn packets", host_vblank_count(),
                calls, packets);
    }
    teardown();
    return 0;
}
