/* Frame-loop checks, run at startup when HMBTN_SELFTEST=1. */

#include <host.h>
#include <libcd.h>
#include <libetc.h>
#include <libgpu.h>

#include <SDL.h>
#include <GL/gl.h>

#include <string.h>

static POLY_F4 check_quad;
static int callback_calls;
static int inner_vsync_result;

static int check(int ok, const char *what) {
    SDL_Log("frame check %s: %s", ok ? "ok" : "FAILED", what);
    return ok ? 0 : 1;
}

static void read_center(unsigned char rgb[3]) {
    unsigned char rgba[4];
    int w;
    int h;

    SDL_GL_GetDrawableSize(host_window, &w, &h);
    glReadPixels(w / 2, h / 2, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    memcpy(rgb, rgba, 3);
}

static void red_quad_callback(void) {
    memset(&check_quad, 0, sizeof(check_quad));
    setPolyF4(&check_quad);
    setRGB0(&check_quad, 220, 30, 30);
    setXY4(&check_quad, 100, 80, 220, 80, 100, 160, 220, 160);
    termPrim(&check_quad);
    DrawOTag((u_long *)&check_quad);
}

static void reentry_callback(void) {
    callback_calls++;
    inner_vsync_result = VSync(0);
}

static int check_cd(void) {
    int failed;

    failed = check(CdReadSync(0, 0) == 0, "CdReadSync(0, 0) reports done");
    failed |= check(CdSync(0, 0) == CdlComplete, "CdSync reports complete");
    failed |= check(CdControl(CdlSetmode, 0, 0) == 1 && CdControlB(CdlSetmode, 0, 0) == 1,
                    "CdControl and CdControlB report success");
    failed |= check(DrawSync(0) == 0 && DrawSync(1) == 0, "DrawSync reports an idle GPU");
    return failed;
}

int host_frame_self_check(void) {
    unsigned char rgb[3];
    Uint64 start;
    Uint64 elapsed_ms;
    int before;
    int failed;
    int i;

    failed = check_cd();

    host_clear();
    VSyncCallback(NULL);
    failed |= check(host_render_vblank() == 1, "before the game draws, a vblank has a frame to present");
    read_center(rgb);
    failed |= check(rgb[1] > 180 && rgb[0] < 80, "before the game draws, the green quad is drawn");
    host_clear();

    VSyncCallback(red_quad_callback);
    failed |= check(host_render_vblank() == 1, "a vblank with a game DrawOTag has a frame to present");
    read_center(rgb);
    failed |= check(rgb[0] > 180 && rgb[1] < 80, "a callback DrawOTag replaces the green quad");
    host_clear();

    VSyncCallback(NULL);
    failed |= check(host_render_vblank() == 0, "after the game has drawn, a vblank without a draw presents nothing");
    read_center(rgb);
    failed |= check(!(rgb[1] > 180 && rgb[0] < 80), "after the game has drawn, the green quad is not drawn");
    host_clear();
    host_debug_reset_game_drawn();

    VSyncCallback(reentry_callback);
    callback_calls = 0;
    before = host_vblank_count();
    VSync(0);
    failed |= check(callback_calls == 1, "VSync inside the callback does not run the callback again");
    failed |= check(inner_vsync_result == before + 1, "VSync inside the callback returns the counter");
    failed |= check(host_vblank_count() == before + 1, "the outer VSync completes one vblank");
    VSyncCallback(NULL);

    failed |= check(VSync(-1) == VSync(-1), "VSync(-1) does not advance");
    start = SDL_GetPerformanceCounter();
    for (i = 0; i < 10; i++) {
        VSync(3);
    }
    elapsed_ms = (SDL_GetPerformanceCounter() - start) * 1000 / SDL_GetPerformanceFrequency();
    SDL_Log("frame check: ten VSync(3) took %u ms", (unsigned)elapsed_ms);
    failed |= check(elapsed_ms >= 450 && elapsed_ms <= 600, "ten VSync(3) take 0.45 to 0.6 seconds");

    return failed;
}
