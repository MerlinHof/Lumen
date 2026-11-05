/* App-owned module; assembled by apps/host/compose.h. */
static void clock_face(int fullscreen) {
    time_t now = time(NULL);
    struct tm t;
    localtime_r(&now, &t);
    int cy = fullscreen ? 390 : 355;
    int r = fullscreen ? 220 : 168;
    if (clock_style < 2) {
        if (!clock_style) ring(300, cy, r, 3);
        for (int i = 0; i < 60; i++) {
            if (clock_style && i % 5) continue;
            double a = i * 6 * .01745329252;
            int inner = r - (i % 5 ? 13 : 25);
            stroke(300 + sin(a)*inner, cy - cos(a)*inner,
                   300 + sin(a)*(r-5), cy - cos(a)*(r-5), i % 5 ? 2 : 4);
        }
        double minute = t.tm_min * .104719755, hour = (t.tm_hour + t.tm_min / 60.) * .523598776;
        stroke(300, cy, 300 + sin(hour)*r*.52, cy - cos(hour)*r*.52, 8);
        stroke(300, cy, 300 + sin(minute)*r*.74, cy - cos(minute)*r*.74, 4);
        aa_disc(300, cy, 7, 0);
    } else {
        char text[24];
        strftime(text, sizeof(text), "%H:%M", &t);
        if (clock_style == 3) {
            aa_soft_rect(60, cy-83, 480, 166, 20, 0);
            int ink = ink_color;
            ink_color = 255;
            centered(60, cy-83, 480, 166, text, ui_font(UI_DISPLAY));
            ink_color = ink;
        } else {
            type_centered(60, cy-68, 480, 136, text, UI_DISPLAY);
        }
    }
    char date[32];
    strftime(date, sizeof(date), "%d.%m.%Y", &t);
    type_centered(60, cy + r + 18, 480, 50, date, UI_SECONDARY);
}
static void app_timer(void) {
    heading_width("Uhr", 128);
    const char *tabs[] = {"Uhr", "Timer", "Wecker"};
    segments(166, 74, 406, 44, tabs, 3, timer_panel);
    char b[80];
    if (!timer_panel) {
        static const char *styles[] = {"Klassisch", "Minimal", "Digital", "Digital · Dunkel"};
        dropdown_button(28, 137, 250, 44, styles[clock_style]);
        icon_button(528, 137, IC_FULLSCREEN);
        clock_face(0);
        return;
    }
    if (timer_panel == 1) {
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
        action_button(28, 566, 260, 64,
                 timer_due      ? "Pause"
                 : timer_paused ? "Fortsetzen"
                                : "Timer starten");
        button_h(312, 566, 260, 64, "Zurücksetzen", 0);
    } else {
        centered(28, 225, 544, 36, "Weckzeit", ui_font(UI_SECONDARY));
        snprintf(b, sizeof(b), "%02d:%02d", alarm_hour, alarm_minute);
        type_centered(100, 265, 400, 104, b, UI_DISPLAY);
        button_h(153, 380, 294, 48, "Weckzeit einstellen", 0);
        label_at(30, 442, 430, 48, tr("Wecker aktiv"), ui_font(UI_PRIMARY));
        toggle(492, 450, alarm_enabled);
        wrapped(30, 535, "Einmaliger Wecker zur eingestellten Uhrzeit.", ui_font(UI_BODY), 540, 3);
    }
    wrapped(30, 664, "Visuelles Signal beim Lesen. Im Tiefschlaf kein Wecksignal.", ui_font(UI_SECONDARY), 540,
            3);
}
