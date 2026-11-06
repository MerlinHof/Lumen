/* App-owned module; assembled by apps/host/compose.h. */
static int reading_streak(void) {
    time_t now = time(NULL);
    struct tm t;
    localtime_r(&now, &t);
    t.tm_hour = 12;
    t.tm_min = t.tm_sec = 0;
    int streak = 0;
    for (int i = 0; i < 31; i++) {
        time_t d = mktime(&t);
        ReadingDay *day = app_day(app_date_key(d), 0);
        if (day && day->seconds)
            streak++;
        else if (i > 0)
            break;
        t.tm_mday--;
    }
    return streak;
}
static void app_insights(void) {
    heading("Lesestatistik");
    const char *tabs[] = {"Überblick", "Bücher", "30 Tage"};
    segments(28, 128, 544, 42, tabs, 3, stats_page);
    if (stats_page == 0) {
        ReadingDay *today = app_day(app_date_key(time(NULL)), 0);
        unsigned mins = today ? today->seconds / 60 : 0;
        char b[100];
        snprintf(b, sizeof(b), "%u / %d min", mins, reading_goal);
        stat_card(28, 196, "Heute", b);
        snprintf(b, sizeof(b), "%d %s", reading_streak(), tr("Tage"));
        stat_card(312, 196, "Leseserie", b);
        snprintf(b, sizeof(b), "%u h %02u m", app_data.seconds / 3600, app_data.seconds / 60 % 60);
        stat_card(28, 326, "Lesezeit", b);
        snprintf(b, sizeof(b), "%u", insights.sessions);
        stat_card(312, 326, "Lesesitzungen", b);
        progress(28, 456, 544, (int)mins, reading_goal);
        drawtext(30, 495, "Mein Tagesziel", ui_font(UI_BODY), 330);
        button_h(390, 484, 182, 44, "Ändern", 0);
        if (current >= 0) {
            BookInsight *r = insight_get(library[current].id, 0);
            int remaining = engine_count() - page - 1;
            snprintf(b, sizeof(b), "%s: %d %s", tr("Aktuelles Buch"), remaining,
                     tr("Seiten übrig"));
            drawtext(30, 562, b, ui_font(UI_BODY), 540);
            if (r && r->pages >= 3) {
                snprintf(b, sizeof(b), "~ %u min %s",
                         (unsigned)((double)r->seconds / r->pages * remaining / 60),
                         tr("bis zum Ende"));
                drawtext(30, 607, b, ui_font(UI_BODY), 540);
            }
        }
        time_t now = time(NULL);
        struct tm t;
        localtime_r(&now, &t);
        t.tm_hour = 12;
        unsigned week = 0, goals = 0;
        for (int d = 0; d < 7; d++) {
            ReadingDay *day = app_day(app_date_key(mktime(&t)), 0);
            if (day) {
                week += day->seconds / 60;
                goals += day->seconds >= (unsigned)reading_goal * 60;
            }
            t.tm_mday--;
        }
        char motivation[160];
        snprintf(motivation, sizeof(motivation), "%u min %s · %u / 7 %s", week, tr("diese Woche"),
                 goals, tr("Tagesziele"));
        drawtext(30, 650, motivation, ui_font(UI_BODY), 540);
        snprintf(motivation, sizeof(motivation),
                 mins >= (unsigned)reading_goal ? "%s" : "%s %u min",
                 tr(mins >= (unsigned)reading_goal ? "Tagesziel erreicht. Schön gelesen!"
                                                   : "Bis zum Tagesziel:"),
                 mins >= (unsigned)reading_goal ? 0 : (unsigned)reading_goal - mins);
        wrapped(30, 691, motivation, ui_font(UI_SECONDARY), 540, 2);
    } else if (stats_page == 1) {
        int rows[BOOK_LIMIT], n = 0;
        BookInsight *row_values[BOOK_LIMIT];
        for (int b = 0; b < library_count && n < BOOK_LIMIT; b++) {
            BookInsight *v = insight_get(library[b].id, 0);
            if (v && (v->seconds || v->pages)) {
                rows[n] = b;
                row_values[n++] = v;
            }
        }
        for (int i = 0; i < n; i++)
            for (int k = i + 1; k < n; k++)
                if (row_values[k]->seconds > row_values[i]->seconds) {
                    int z = rows[i];
                    rows[i] = rows[k];
                    rows[k] = z;
                    BookInsight *value = row_values[i];
                    row_values[i] = row_values[k];
                    row_values[k] = value;
                }
        for (int r = 0; r < 5 && extra_page * 5 + r < n; r++) {
            Book *b = &library[rows[extra_page * 5 + r]];
            BookInsight *v = insight_get(b->id, 0);
            int y = 190 + r * 91;
            drawtext_raw(30, y, b->title, ui_font(UI_BODY), 540);
            char line[120];
            snprintf(line, sizeof(line), "%u min  ·  %u %s  ·  %u %s", v->seconds / 60, v->pages,
                     tr("Seiten"), v->sessions, tr("Sitzungen"));
            drawtext(30, y + 35, line, ui_font(UI_SECONDARY), 540);
            progress(30, y + 66, 540, b->page + 1, b->total);
        }
        if (!n)
            wrapped(30, 220, "Deine Leseaktivität wird ab jetzt pro Buch erfasst.", ui_font(UI_TITLE), 540,
                    4);
        pager(745, &extra_page, (n + 4) / 5);
    } else {
        unsigned secs[30], sum = 0, max = 60, active = 0;
        time_t now = time(NULL);
        struct tm t;
        localtime_r(&now, &t);
        t.tm_hour = 12;
        t.tm_min = t.tm_sec = 0;
        t.tm_mday -= 29;
        for (int i = 0; i < 30; i++) {
            time_t d = mktime(&t);
            ReadingDay *r = app_day(app_date_key(d), 0);
            secs[i] = r ? r->seconds : 0;
            sum += secs[i];
            if (secs[i])
                active++;
            if (secs[i] > max)
                max = secs[i];
            t.tm_mday++;
        }
        char b[80];
        snprintf(b, sizeof(b), "%u %s  ·  %.1f h", active, tr("aktive Tage"), sum / 3600.0);
        drawtext(30, 214, b, ui_font(UI_PRIMARY), 540);
        for (int i = 0; i < 30; i++) {
            int h = (int)(240ULL * secs[i] / max);
            control_surface(30 + i * 18, 565 - h, 12, h ? h : 2, RADIUS_SMALL, secs[i] ? 0 : 220);
        }
        drawtext(30, 590, "Vor 30 Tagen", ui_font(UI_SECONDARY), 260);
        drawtext(490, 590, "Heute", ui_font(UI_SECONDARY), 82);
        snprintf(b, sizeof(b), "Ø %u min / %s", active ? sum / 60 / active : 0, tr("aktivem Tag"));
        drawtext(30, 653, b, ui_font(UI_BODY), 540);
    }
}
