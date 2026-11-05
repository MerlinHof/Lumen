#include "api.h"
static SignalGame signal_game;
static int signal_ready, signal_dirty, signal_frame_dirty = 1;
static unsigned char signal_frame[SIGNAL_W * SIGNAL_H];

static void signal_save(void) {
    if (signal_dirty && save_blob("slide3d-v1.bin", &signal_game, sizeof(signal_game)))
        signal_dirty = 0;
}
static void signal_open(void) {
    if (signal_ready) return;
    signal_ready = 1;
    if (load_blob("slide3d-v1.bin", &signal_game, sizeof(signal_game))) {
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
    heading_actions("Slide 3D", 1);
    icon_button(528, 74, IC_RESTART);
    if (signal_frame_dirty) {
        signal_render(&signal_game, signal_frame);
        signal_frame_dirty = 0;
    }
    memcpy(ui_pixels + 132 * WIDTH, signal_frame, sizeof(signal_frame));
    for (int i = 0; i < 16; i++) {
        if (!signal_game.tiles[i]) continue;
        int x, y;
        signal_project(i, &x, &y);
        char label[8];
        snprintf(label, sizeof(label), "%d", signal_game.tiles[i]);
        type_centered(x-27, y+110, 54, 44, label, UI_HEADING);
    }
    char steps[32];
    snprintf(steps, sizeof(steps), "%d", signal_game.steps);
    type_centered(28, 635, 544, 48, steps, UI_DISPLAY);
    type_centered(28, 680, 544, 24, "Züge", UI_SECONDARY);
    type_centered(28, 707, 544, 46, signal_won(&signal_game) ? "Gewonnen!" :
                  "Ordne die Zahlen von 1 bis 15.", UI_BODY);
}
static int signal_input(const Input *in) {
    signal_open();
    if (in->kind != INPUT_TAP) return 1;
    if (in->x >= 528 && in->y >= 74 && in->y < 118) {
        signal_new(&signal_game, (unsigned)extra_millis(), 40);
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
