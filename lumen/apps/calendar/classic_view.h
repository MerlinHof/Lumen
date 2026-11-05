static int calendar_draw16(void) {
    if (screen != 21)
        return 0;
    calendar_parse16();
    if (calendar_view16 == 2) {
        app_calendar();

        const char *names[] = {"Tag", "Woche", "Monat", "Jahr"};
        dropdown_button(250, 74, 145, 44, names[calendar_view16]);
        return 1;
    }
    heading_actions("Kalender", 2);

    icon_button(468, 74, IC_TODAY);
    icon_button(528, 74, IC_NOTE_PLUS);
    const char *names[] = {"Tag", "Woche", "Monat", "Jahr"};
    dropdown_button(250, 74, 145, 44, names[calendar_view16]);
    char b[100];
    snprintf(b, 100, "%02d.%02d.%04d", calendar_day, calendar_month, calendar_year);
    centered(90, 135, 420, 46, b, ui_font(UI_PRIMARY));
    icon_button(28, 135, IC_PREV);
    icon_button(528, 135, IC_NEXT);
    if (calendar_view16 == 0) {
        static int shown;
        int date = calendar_year * 10000 + calendar_month * 100 + calendar_day;
        if (shown != date) {
            calendar_day_page16 = 0;
            shown = date;
        }
        int entries[2048], count = 0;
        for (int i = 0; i < event_count16; i++)
            if (event_occurs16(&events16[i], date))
                entries[count++] = i;
        if (calendar_day_page16 * 8 >= count)
            calendar_day_page16 = 0;
        for (int row = 0; row < 8 && calendar_day_page16 * 8 + row < count; row++) {
            Event16 *e = &events16[entries[calendar_day_page16 * 8 + row]];
            char text[300];
            if (e->hour >= 0)
                snprintf(text, sizeof(text), "%02d:%02d  %s", e->hour, e->minute, e->title);
            else
                snprintf(text, sizeof(text), "• %s", e->title);
            drawtext_raw(30, 208 + row * 29, text, ui_font(UI_SECONDARY), 540);
        }
        if (!count)
            event_lines16(date, 30, 208, 540, 1);
        divider(28, 470, 544);
        drawtext(30, 490, "Tagesnotiz", ui_font(UI_PRIMARY), 540);
        wrapped_raw(30, 535, calendar_note, ui_font(UI_BODY), 540, 3);
        pager(679, &calendar_day_page16, (count + 7) / 8);
    }
    if (calendar_view16 == 1) {
        time_t start = calendar_time16(calendar_year, calendar_month, calendar_day);
        struct tm t;
        localtime_r(&start, &t);
        t.tm_mday -= (t.tm_wday + 6) % 7;
        for (int n = 0; n < 7; n++) {
            struct tm day = t;
            day.tm_mday += n;
            mktime(&day);
            int y = 200 + n * 70;
            snprintf(b, 100, "%02d.%02d.", day.tm_mday, day.tm_mon + 1);
            drawtext_raw(30, y, b, ui_font(UI_BODY), 95);
            event_lines16(date16(&day), 140, y, 430, 2);
            rect(28, y + 63, 544, 1, 215);
        }
    }
    if (calendar_view16 == 3) {
        for (int m = 1; m <= 12; m++) {
            int x = 28 + (m - 1) % 3 * 185, y = 205 + (m - 1) / 3 * 123;
            region(x, y, 174, 112);
            snprintf(b, 100, "%02d / %d", m, calendar_year);
            centered(x, y, 174, 30, b, ui_font(UI_BODY));
            struct tm t = {
                .tm_year = calendar_year - 1900, .tm_mon = m - 1, .tm_mday = 1, .tm_hour = 12};
            mktime(&t);
            int off = (t.tm_wday + 6) % 7;
            for (int d = 1; d <= calendar_days(calendar_year, m); d++) {
                int cell = off + d - 1, xx = x + 12 + cell % 7 * 22, yy = y + 39 + cell / 7 * 12;
                int has = event_on16(calendar_year * 10000 + m * 100 + d);
                disc(xx, yy, has ? 4 : 2, has ? 0 : 190);
            }
        }
    }
    return 1;
}
