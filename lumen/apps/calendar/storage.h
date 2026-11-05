/* App-owned module; assembled by apps/host/compose.h. */
static void app_calendar_load(void) {
    u17_calendar_dirty();
    calendar_note_days = 0;
    for (int day = 1; day <= calendar_days(calendar_year, calendar_month); day++) {
        char p[2048];
        snprintf(p, sizeof(p), "%s/calendar/%04d%02d%02d.txt", data_root, calendar_year,
                 calendar_month, day);
        struct stat st;
        if (!stat(p, &st) && st.st_size > 0)
            calendar_note_days |= 1u << (day - 1);
    }
    char path[2048];
    snprintf(path, sizeof(path), "%s/calendar/%04d%02d%02d.txt", data_root, calendar_year,
             calendar_month, calendar_day);
    calendar_note[0] = 0;
    FILE *f = fopen(path, "rb");
    if (f) {
        size_t n = fread(calendar_note, 1, sizeof(calendar_note) - 1, f);
        calendar_note[n] = 0;
        fclose(f);
    }
}
static void apps_note_commit(const char *text) {
    u17_calendar_dirty();
    char folder[2048], path[2048];
    snprintf(folder, sizeof(folder), "%s/calendar", data_root);
    mkdir(folder, 0700);
    snprintf(path, sizeof(path), "%s/%04d%02d%02d.txt", folder, calendar_year, calendar_month,
             calendar_day);
    if (!atomic_write(path, text, strlen(text)))
        ui_message("Speichern fehlgeschlagen.");
    else {
        snprintf(calendar_note, sizeof(calendar_note), "%s", text);
        if (*text)
            calendar_note_days |= 1u << (calendar_day - 1);
        else
            calendar_note_days &= ~(1u << (calendar_day - 1));
    }
}
