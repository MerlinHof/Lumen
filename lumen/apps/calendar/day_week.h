/* App-owned module; assembled by apps/host/compose.h. */
static int calendar_layout17(void) {
    if (ui_route() != 21 || calendar_view16 == 2 || calendar_view16 == 3)
        return 0;
    heading_width("Kalender", 194);
    const char *views[] = {"Tag", "Woche", "Monat", "Jahr"};
    dropdown_button(250, 74, 145, 44, views[calendar_view16]);
    icon_button(468, 74, IC_TODAY);
    icon_button(528, 74, IC_NOTE_PLUS);
    icon_button(28, 136, IC_PREV);
    icon_button(528, 136, IC_NEXT);
    time_t stamp = calendar_time16(calendar_year, calendar_month, calendar_day);
    struct tm t;
    localtime_r(&stamp, &t);
    char label[160];
    if (calendar_view16 == 1) {
        char week[12], year[12];
        strftime(week, 12, "%V", &t);
        strftime(year, 12, "%G", &t);
        snprintf(label, sizeof(label), tr("Woche %s · %s"), week, year);
        centered(90, 136, 420, 44, label, ui_font(UI_PRIMARY));
        t.tm_mday -= (t.tm_wday + 6) % 7;
        const char *days[] = {"Wochentag.Mo", "Wochentag.Di", "Wochentag.Mi", "Wochentag.Do",
                              "Wochentag.Fr", "Wochentag.Sa", "Wochentag.So"};
        for (int col = 0; col < 7; col++) {
            struct tm day = t;
            day.tm_mday += col;
            mktime(&day);
            int x = 28 + col * 78;
            centered(x, 200, 78, 35, days[col], ui_font(UI_SECONDARY));
            snprintf(label, 160, "%02d.%02d.", day.tm_mday, day.tm_mon + 1);
            centered(x, 235, 78, 38, label, ui_font(UI_SECONDARY));
            if (date16(&day) == app_date_key(time(NULL)))
                rect(x + 14, 274, 50, 3, 0);
            rect(x, 289, 1, 370, 205);
            region(x, 200, 78, 460);
            event_lines16(date16(&day), x + 7, 306, 64, 12);
        }
        rect(572, 289, 1, 370, 205);
        rect(28, 288, 544, 2, 180);
        rect(28, 658, 544, 2, 180);
        centered(28, 683, 544, 36, "Tag antippen zum Öffnen", ui_font(UI_SECONDARY));
    } else {
        const char *months[] = {"Januar",    "Februar", "März",     "April",
                                "Mai",       "Juni",    "Juli",     "August",
                                "September", "Oktober", "November", "Dezember"};
        const char *weekdays[] = {"Sonntag",    "Montag",  "Dienstag", "Mittwoch",
                                  "Donnerstag", "Freitag", "Samstag"};
        snprintf(label, sizeof(label), "%s %d", tr(months[calendar_month - 1]), calendar_year);
        centered(90, 136, 420, 44, label, ui_font(UI_BODY));
        snprintf(label, sizeof(label), "%02d", calendar_day);
        drawtext_raw(30, 208, label, ui_font(UI_DISPLAY), 94);
        drawtext(132, 208, weekdays[t.tm_wday], ui_font(UI_PRIMARY), 425);
        snprintf(label, sizeof(label), "%02d.%02d.%04d", calendar_day, calendar_month,
                 calendar_year);
        ink_color = UI_SECONDARY_INK;
        drawtext_raw(134, 253, label, ui_font(UI_SECONDARY), 400);
        ink_color = 0;
        divider(28, 294, 544);
        drawtext(30, 315, "Tagesnotiz", ui_font(UI_TITLE), 480);
        if (*calendar_note) {
            static int date;
            int key = date16(&t);
            if (date != key) {
                calendar_day_page16 = 0;
                date = key;
            }
            const char *p = calendar_note;
            int lines = 0;
            while (*p) {
                p = md_next(p, 540, ui_font(UI_PRIMARY));
                lines++;
            }
            int pages = (lines + 7) / 8;
            if (calendar_day_page16 >= pages)
                calendar_day_page16 = 0;
            p = calendar_note;
            for (int i = 0; i < calendar_day_page16 * 8 && *p; i++)
                p = md_next(p, 540, ui_font(UI_PRIMARY));
            const char *end = p;
            for (int i = 0; i < 8 && *end; i++)
                end = md_next(end, 540, ui_font(UI_PRIMARY));
            char text[2048];
            snprintf(text, sizeof(text), "%.*s", (int)(end - p), p);
            wrapped_raw(30, 372, text, ui_font(UI_PRIMARY), 540, 8);
            pager(745, &calendar_day_page16, pages);
        } else {
            ink_color = UI_SECONDARY_INK;
            wrapped(30, 379, "Noch keine Notiz für diesen Tag.", ui_font(UI_BODY), 540, 3);
            drawtext(30, 460, "Mit + eine Notiz hinzufügen.", ui_font(UI_SECONDARY), 540);
            ink_color = 0;
        }
    }

    return 1;
}
