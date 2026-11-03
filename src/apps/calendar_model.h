/* Local day notes only. No cloud accounts, protocols or network calendar jobs. */
typedef struct {
    int date, hour, minute;
    char title[512];
} Event16;
static Event16 events16[2048];
static int event_count16;
static int date16(struct tm *t) {
    return (t->tm_year + 1900) * 10000 + (t->tm_mon + 1) * 100 + t->tm_mday;
}
static time_t calendar_time16(int y, int m, int d) {
    struct tm t = {.tm_year = y - 1900, .tm_mon = m - 1, .tm_mday = d, .tm_hour = 12};
    return mktime(&t);
}
static void u17_calendar_dirty(void) { calendar_loaded16 = 0; }
static void calendar_parse16(void) {
    if (calendar_loaded16)
        return;
    calendar_loaded16 = 1;
    event_count16 = 0;
    char dir[2048];
    snprintf(dir, sizeof(dir), "%s/calendar", data_root);
    DIR *d = opendir(dir);
    struct dirent *e;
    while (d && (e = readdir(d)) && event_count16 < 2048) {
        int key, n = 0;
        if (sscanf(e->d_name, "%8d.txt%n", &key, &n) != 1 || n != 12 || e->d_name[n])
            continue;
        char p[4096];
        snprintf(p, sizeof(p), "%s/%s", dir, e->d_name);
        FILE *f = fopen(p, "r");
        if (!f)
            continue;
        Event16 *v = &events16[event_count16];
        memset(v, 0, sizeof(*v));
        v->date = key;
        v->hour = -1;
        size_t len = fread(v->title, 1, 511, f);
        v->title[len] = 0;
        fclose(f);
        if (*v->title)
            event_count16++;
    }
    if (d)
        closedir(d);
}
static int event_occurs16(const Event16 *e, int date) { return e->date == date; }
static int event_on16(int date) {
    calendar_parse16();
    for (int i = 0; i < event_count16; i++)
        if (events16[i].date == date)
            return 1;
    return 0;
}
static void event_lines16(int date, int x, int y, int width, int max) {
    calendar_parse16();
    for (int i = 0; i < event_count16; i++)
        if (events16[i].date == date) {
            wrapped_raw(x, y, events16[i].title, &caption, width, max);
            return;
        }
    ink_color = 140;
    centered(x, y, width, 32, "—", &caption);
    ink_color = 0;
}
