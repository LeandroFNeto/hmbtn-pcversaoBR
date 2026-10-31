/* libetc: VSync is the host frame boundary. Each vblank a blocking VSync waits runs the
 * registered callback, polls window events, presents, and paces to 59.94 Hz. */

#include <libetc.h>
#include <host.h>
#include <host_input.h>
#include <libgpu.h>

#include <SDL.h>

#include <stddef.h>

#define VBLANK_HZ_NUM 60000
#define VBLANK_HZ_DEN 1001
#define VBLANK_MAX_LATE 4

static void (*vsync_callback)(void);
static int in_callback;
static int vblank_count;
static Uint64 next_deadline;

int host_vblank_count(void) {
    return vblank_count;
}

const int *host_vblank_counter(void) {
    return &vblank_count;
}

static int game_has_drawn;

int host_render_vblank(void) {
    int drew;

    if (vsync_callback != NULL && !in_callback) {
        in_callback = 1;
        vsync_callback();
        in_callback = 0;
    }
    drew = gpu_take_game_drew();
    if (drew) {
        game_has_drawn = 1;
    }
    if (!game_has_drawn) {
        gpu_draw_demo();
        return 1;
    }
    return drew;
}

int host_game_has_drawn(void) {
    return game_has_drawn;
}

void host_debug_reset_game_drawn(void) {
    game_has_drawn = 0;
}

static void poll_events(void) {
    SDL_Event event;

    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            host_quit();
        }
        host_input_handle_event(&event);
    }
}

static void pace(void) {
    Uint64 freq;
    Uint64 period;
    Uint64 now;
    Uint64 remaining_ms;

    freq = SDL_GetPerformanceFrequency();
    period = freq * VBLANK_HZ_DEN / VBLANK_HZ_NUM;
    now = SDL_GetPerformanceCounter();
    if (next_deadline == 0) {
        next_deadline = now;
    }
    next_deadline += period;
    if (now > next_deadline + VBLANK_MAX_LATE * period) {
        next_deadline = now;
        return;
    }
    while ((now = SDL_GetPerformanceCounter()) < next_deadline) {
        remaining_ms = (next_deadline - now) * 1000 / freq;
        SDL_Delay(remaining_ms > 1 ? (Uint32)(remaining_ms - 1) : 0);
    }
}

static void host_vblank(void) {
    int drew;

    vblank_count++;
    drew = host_render_vblank();
    poll_events();
    host_input_sample();
    if (drew) {
        host_present();
    }
    pace();
}

int VSync(int mode) {
    int count;

    if (mode < 0 || in_callback) {
        return vblank_count;
    }
    if (mode == 1) {
        return 0;
    }
    count = mode == 0 ? 1 : mode;
    while (count-- > 0) {
        host_vblank();
    }
    return vblank_count;
}

int VSyncCallback(void (*f)(void)) {
    vsync_callback = f;
    return 0;
}

/* As in PSY-Q, only the first call initializes the callbacks; later calls do nothing.
 * afile.c calls it before every read, after the VSync callback is installed. */
void ResetCallback(void) {
    static int initialized;

    if (initialized) {
        return;
    }
    initialized = 1;
    vsync_callback = NULL;
}

int SetVideoMode(long mode) {
    (void)mode;
    return MODE_NTSC;
}

long GetVideoMode(void) {
    return MODE_NTSC;
}
