#include "api.h"
#include "../../system/app_runtime.h"
#include "../../system/app_storage.h"
#include "../../system/ui/paint.h"
#include <stdio.h>
#include <time.h>
static SignalGame signal_game;
static int signal_ready, signal_dirty, signal_frame_dirty = 1;
static unsigned char signal_frame[SIGNAL_W * SIGNAL_H];

static void signal_save(void) {
    if (signal_dirty && app_state_save("slide3d-v1.bin", &signal_game, sizeof(signal_game)))
        signal_dirty = 0;
}
static void signal_open(void) {
    if (signal_ready) return;
    signal_ready = 1;
    if (app_state_load("slide3d-v1.bin", &signal_game, sizeof(signal_game))) {
        unsigned seen = 0;
        for (int i = 0; i < 16; i++)
            if (signal_game.tiles[i] >= 0 && signal_game.tiles[i] < 16)
                seen |= 1u << signal_game.tiles[i];
        if (seen == 65535 && signal_game.empty >= 0 && signal_game.empty < 16 &&
            !signal_game.tiles[signal_game.empty] && signal_game.steps >= 0)
            return;
    }
    signal_new(&signal_game, (unsigned)time(NULL), 20);
}
static void signal_draw(void) {
    signal_open();
    ui_paint_heading("Slide 3D", 1);
    ui_paint_icon_button(528, 74, IC_RESTART);
    if (signal_frame_dirty) {
        signal_render(&signal_game, signal_frame);
        signal_frame_dirty = 0;
    }
    ui_paint_image(0, 132, SIGNAL_W, SIGNAL_H, signal_frame);
    for (int i = 0; i < 16; i++) {
        if (!signal_game.tiles[i]) continue;
        int x, y;
        signal_project(i, &x, &y);
        char label[8];
        snprintf(label, sizeof(label), "%d", signal_game.tiles[i]);
        ui_paint_text(x-27, y+110, 54, 44, label, PAINT_HEADING);
    }
    char steps[32];
    snprintf(steps, sizeof(steps), "%d", signal_game.steps);
    ui_paint_text(28, 635, 544, 48, steps, PAINT_DISPLAY);
    ui_paint_text(28, 680, 544, 24, "Züge", PAINT_SECONDARY);
    ui_paint_text(28, 707, 544, 46, signal_won(&signal_game) ? "Gewonnen!" :
                  "Ordne die Zahlen von 1 bis 15.", PAINT_BODY);
}
static int signal_input(const Input *in) {
    signal_open();
    if (in->kind != INPUT_TAP) return 1;
    if (in->x >= 528 && in->y >= 74 && in->y < 118) {
        signal_new(&signal_game, (unsigned)ui_clock_millis(), 40);
    } else {
        int closest = -1, distance = 45 * 45;
        for (int i = 0; i < 16; i++) {
            int x, y;
            signal_project(i, &x, &y);
            int dx = x - in->x, dy = y + 132 - in->y;
            int square = dx * dx + dy * dy;
            if (square < distance) {
                distance = square;
                closest = i;
            }
        }
        if (signal_won(&signal_game) || !signal_slide(&signal_game, closest)) return 1;
    }
    signal_dirty = signal_frame_dirty = 1;
    return 1;
}

void slide_3d_register(void) {
    app_page_register(APP_SIGNAL, signal_draw, signal_input, signal_save);
}
