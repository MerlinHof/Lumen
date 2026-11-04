/* App-owned module; assembled by apps/host/compose.h. */
static void app_calendar(void) {
    heading_width("Kalender", 194);
    icon_button(468, 74, IC_TODAY);
    icon_button(528, 74, IC_NOTE_PLUS);
    const char *months[] = {"Januar", "Februar", "März",      "April",   "Mai",      "Juni",
                            "Juli",   "August",  "September", "Oktober", "November", "Dezember"};
    char label[80];
    snprintf(label, sizeof(label), "%s %d", tr(months[calendar_month - 1]), calendar_year);
    region(92, 130, 416, 44);
    centered(92, 130, 416, 44, label, ui_font(UI_TITLE));
    stroke(477, 149, 483, 155, 2);
    stroke(483, 155, 489, 149, 2);
    icon_button(28, 130, IC_PREV);
    icon_button(528, 130, IC_NEXT);
    const char *weekdays[] = {"Wochentag.Mo", "Wochentag.Di", "Wochentag.Mi", "Wochentag.Do",
                              "Wochentag.Fr", "Wochentag.Sa", "Wochentag.So"};
    for (int i = 0; i < 7; i++)
        centered(27 + i * 78, 194, 78, 34, weekdays[i], ui_font(UI_SECONDARY));
    divider(28, 227, 544);
    struct tm first = {
        .tm_year = calendar_year - 1900, .tm_mon = calendar_month - 1, .tm_mday = 1, .tm_hour = 12};
    mktime(&first);
    int offset = (first.tm_wday + 6) % 7, days = calendar_days(calendar_year, calendar_month),
        today = app_date_key(time(NULL));
    for (int d = 1; d <= days; d++) {
        int cell = offset + d - 1, x = 27 + (cell % 7) * 78, y = 234 + (cell / 7) * 58;
        region(x, y, 78, 56);
        char b[8];
        snprintf(b, sizeof(b), "%d", d);
        if (d == calendar_day)
            aa_disc(x + 39, y + 24, 22, 0);
        else if (calendar_year * 10000 + calendar_month * 100 + d == today)
            ring(x + 39, y + 24, 22, 2);
        ink_color = d == calendar_day ? 255 : (cell % 7 >= 5 ? 112 : 0);
        centered(x, y, 78, 48, b, ui_font(UI_BODY));
        ink_color = 0;
        int key = calendar_year * 10000 + calendar_month * 100 + d;
        if ((calendar_note_days & (1u << (d - 1))) || event_on16(key))
            disc(x + 61, y + 9, 2, 0);

        ReadingDay *r = app_day(key, 0);
        if (r && r->seconds)
            rect(x + 29, y + 55, 20, 2, 160);
    }
    divider(28, 592, 544);
    snprintf(label, sizeof(label), "%02d.%02d.%04d", calendar_day, calendar_month, calendar_year);
    ReadingDay *activity = app_day(calendar_year * 10000 + calendar_month * 100 + calendar_day, 0);
    if (activity) {
        char activity_label[100];
        snprintf(activity_label, sizeof(activity_label), "%s  ·  %u min", label,
                 activity->seconds / 60);
        drawtext(30, 601, activity_label, ui_font(UI_PRIMARY), 375);
    } else
        drawtext(30, 601, label, ui_font(UI_PRIMARY), 375);
    wrapped_raw(30, 642, *calendar_note ? calendar_note : tr("Keine Tagesnotiz"), ui_font(UI_SECONDARY), 370, 2);

    if (*calendar_note)
        icon_button(28, 681, IC_TRASH);
}
