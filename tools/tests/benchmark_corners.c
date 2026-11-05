/* Benchmarks actual UI frames and the shared surface primitive without I/O. */
#include "../../lumen/system/ui/ui.c"
#include <assert.h>
static uint64_t nanos(void) {
    struct timespec t;
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t);
    return (uint64_t)t.tv_sec * 1000000000 + (uint64_t)t.tv_nsec;
}
static void pick_scene(int scene) {
    static const char *choices[] = {"Klassisch", "Minimal", "Digital", "Digital · Dunkel",
                                    "Frosch-Solitär", "Slide 3D"};
    dropdown = 0;
    route_to(scene == 0 ? 27 : 28);
    timer_panel = scene == 1 ? 1 : 0;
    if (scene == 3) open_dropdown(902, choices, 6, 0, 28, 185, 250);
}
int main(void) {
    data_root = "build/corner-benchmark/data";
    assert(engine_init());
    ui_init();
    for (int profile = 0; profile < 3; profile++) {
        control_corner_profile = profile;
        for (int scene = 0; scene < 4; scene++) {
            pick_scene(scene);
            ui_draw();
        }
    }
    for (int run = 0; run < 9; run++) {
        for (int offset = 0; offset < 3; offset++) {
            int profile = (run + offset) % 3;
            control_corner_profile = profile;
            uint64_t elapsed = 0;
            for (int scene = 0; scene < 4; scene++) {
                pick_scene(scene);
                uint64_t begin = nanos();
                for (int frame = 0; frame < 70; frame++) ui_draw();
                elapsed += nanos() - begin;
            }
            uint64_t begin = nanos();
            for (int i = 0; i < 10000; i++) {
                int x = 24 + (i % 5) * 22, y = 32 + (i % 7) * 41;
                control_surface(x, y, 170, 48, 13, 0);
                control_surface(x+2, y+2, 166, 44, 11, 255);
            }
            uint64_t primitive = nanos() - begin;
            printf("%d %llu %llu\n", profile, (unsigned long long)elapsed,
                   (unsigned long long)primitive);
        }
    }
    engine_shutdown();
}
