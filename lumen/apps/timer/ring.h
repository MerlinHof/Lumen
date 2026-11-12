/* App-owned module; assembled by apps/host/compose.h. */
static void v1_timer_ring(int fullscreen) {
    int64_t remain = timer_due      ? timer_due - milliseconds()
                     : timer_paused ? timer_paused
                                    : (int64_t)timer_minutes * 60000;
    if (remain < 0)
        remain = 0;
    int total = timer_total_seconds > 0 ? timer_total_seconds : timer_minutes * 60;
    double f = (timer_due || timer_paused) ? fmin(1, remain / (total * 1000.)) : 1;
    int cy = fullscreen ? 370 : 278,
        r = fullscreen ? 215 : 72; /* Sparse ring, only updated on changed seconds. */
    aa_arc_color(300, cy, r, 0, 360, fullscreen ? 12 : 10, 205);
    int end = 270 + (int)(360 * f);
    if (f >= .9999)
        ring(300, cy, r, fullscreen ? 12 : 10);
    else if (end <= 360)
        aa_arc(300, cy, r, 270, end, fullscreen ? 12 : 10);
    else {
        aa_arc(300, cy, r, 270, 360, fullscreen ? 12 : 10);
        aa_arc(300, cy, r, 0, end - 360, fullscreen ? 12 : 10);
    }
    char b[64];
    long secs = (remain + 999) / 1000;
    snprintf(b, sizeof(b), "%02ld:%02ld", secs / 60, secs % 60);
    centered(28, cy - 45, 544, 90, b, fullscreen ? ui_font(UI_DISPLAY) : ui_font(UI_TITLE));
    if (fullscreen) {
        centered(28, cy + 66, 544, 36, "Verbleibende Zeit", ui_font(UI_SECONDARY));
        lumen_button_action("start-pause",28,665,260,60,
                 tr(timer_due      ? "Pause"
                 : timer_paused ? "Fortsetzen"
                                : "Timer starten"),
                 0);
        lumen_button_action("reset",312,665,260,60,tr("Zurücksetzen"),0);
    }
}
