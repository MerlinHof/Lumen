/* App-owned module; assembled by apps/host/compose.h. */
static void app_timer(void) {
    heading("Timer & Wecker");
    const char *tabs[] = {"Timer", "Wecker"};
    segments(28, 133, 544, 44, tabs, 2, timer_panel);
    char b[80];
    if (!timer_panel) {
        extra_timer_label(b, sizeof(b));
        if (!*b)
            snprintf(b, sizeof(b), "%02d:00", timer_minutes);
        v1_timer_ring(0);
        icon_button(528, 185, IC_FULLSCREEN);
        const int presets[] = {5, 10, 20, 30};
        for (int i = 0; i < 4; i++) {
            snprintf(b, sizeof(b), "%d min", presets[i]);
            button_h(28 + i * 140, 397, 124, 48, b, !timer_due && timer_minutes == presets[i]);
        }
        button_h(28, 472, 80, 52, "−", 0);
        snprintf(b, sizeof(b), "%d min", timer_minutes);
        button_h(122, 472, 356, 52, b, 0);
        button_h(492, 472, 80, 52, "+", 0);
        button_h(28, 566, 260, 64,
                 timer_due      ? "Pause"
                 : timer_paused ? "Fortsetzen"
                                : "Timer starten",
                 0);
        button_h(312, 566, 260, 64, "Zurücksetzen", 0);
    } else {
        centered(28, 225, 544, 36, "Weckzeit", ui_font(UI_SECONDARY));
        snprintf(b, sizeof(b), "%02d:%02d", alarm_hour, alarm_minute);
        button_h(100, 292, 400, 94, b, 0);
        label_at(30, 442, 430, 48, tr("Wecker aktiv"), ui_font(UI_PRIMARY));
        toggle(492, 450, alarm_enabled);
        wrapped(30, 535, "Einmaliger Wecker zur eingestellten Uhrzeit.", ui_font(UI_BODY), 540, 3);
    }
    wrapped(30, 664, "Visuelles Signal beim Lesen. Im Tiefschlaf kein Wecksignal.", ui_font(UI_SECONDARY), 540,
            3);
}
