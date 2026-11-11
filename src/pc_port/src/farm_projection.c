/* After "save and sleep" the farm comes back from the save overlay with its start-up already
 * finished, so the step that sets the GTE screen offset (camera_init_projection) never runs again,
 * while main_frame_reset has put the offset back to (0, 0). The whole view then sits 160 x 120
 * pixels up and to the left. The PS1 overlay reload runs that step again; the host does not
 * reload the overlay. This restores the farm's projection when the farm is past start-up and the
 * offset is (0, 0). */

#include <gte.h>
#include <host.h>

#include <SDL.h>

void __real_farm_enter_dispatch(void);
void camera_init_projection(void);
void scene_set_projection(void);
extern unsigned int farm_startup_phase;

#define FARM_STARTUP_DONE 15

void __wrap_farm_enter_dispatch(void) {
    GteSettings settings;

    if (farm_startup_phase >= FARM_STARTUP_DONE) {
        gte_debug_settings(&settings);
        if (settings.off_x == 0 && settings.off_y == 0) {
            camera_init_projection();
            scene_set_projection();
            SDL_Log("farm projection restored: offset 160,120");
        }
    }
    __real_farm_enter_dispatch();
}
