/* App-owned module; assembled by apps/host/compose.h. */
static void v1_open(int target) {
    polish_open(target);
    v1_init();
    v1_page = 0;
    if (target == 87 && !timer_due && !timer_paused)
        timer_total_seconds = timer_minutes * 60;
    if (target == 93)
        open_keyboard(130, "Tastatur", "");
}
static void v1_draw(void) {
    switch (ui_route()) {
    case 93:
        heading("Tastatur");
        button_h(28, 180, 544, 60, "Text eingeben", 0);
        break;
    }
    if (v1_delete) {
        rect(21, 267, 564, 236, 190);
        rect(17, 263, 564, 236, 255);
        key_outline(17, 263, 564, 236);
        centered(28, 285, 544, 55, "Wirklich löschen?", ui_font(UI_TITLE));
        button_h(36, 400, 250, 58, "Abbrechen", 0);
        button_h(310, 400, 250, 58, "Löschen", 0);
    }
}
static int v1_commit(int target, const char *text) {
    if (target == 130)
        return 1;
    return 0;
}
static int v1_tick(void) {
    if (polish_tick())
        return 1;
    if (alarm_ring) {
        if (!timer_flash_started) {
            timer_flash_started = extra_millis();
            timer_last_phase = -1;
        }
        int64_t phase = (extra_millis() - timer_flash_started) / 500;
        if (phase < 120 && phase != timer_last_phase) {
            timer_last_phase = phase;
            return 1;
        }
    } else
        timer_flash_started = 0;
    static int lastsecond;
    int sec = timer_due ? (timer_due - extra_millis()) / 1000 : 0;
    if ((ui_route() == 87 || ui_route() == 28) && sec != lastsecond) {
        lastsecond = sec;
        return 1;
    }
    return 0;
}
static int v1_poll(void) {
    if (alarm_ring && timer_flash_started && extra_millis() - timer_flash_started < 60000)
        return 500;
    if (ui_route() == 87)
        return 1000;
    return -1;
}
static void v1_effect(void) {
    if (alarm_ring && timer_flash_started && extra_millis() - timer_flash_started < 60000 &&
        (timer_last_phase & 1))
        for (size_t i = 0; i < sizeof(ui_pixels); i++)
            ui_pixels[i] = 255 - ui_pixels[i];
}
static int v1_input(const Input *in){(void)in;return 2;}
