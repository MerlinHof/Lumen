/* App-owned module; assembled by apps/host/compose.h. */
static void apps_init(void) {
    char path[2048];
    snprintf(path, sizeof(path), "%s/apps-v1.bin", data_root);
    memset(&app_data, 0, sizeof(app_data));
    FILE *f = fopen(path, "rb");
    if (f) {
        AppData tmp;
        if (fread(&tmp, sizeof(tmp), 1, f) == 1 && tmp.magic == 0x4c415050 && tmp.version == 1) {
            app_data = tmp;
        }
        fclose(f);
    }
    app_data.magic = 0x4c415050;
    app_data.version = 1;
    app_last_time = app_millis();

}
static void apps_checkpoint(void) {
    int64_t now = app_millis(), delta = (now - app_last_time) / 1000;
    if (delta < 0)
        delta = 0;
    if (delta > 300)
        delta = 300;
    if (ui_route() == 2 && !sleeping && delta > 0) {
        ReadingDay *d = app_day(app_date_key(time(NULL)), 1);
        d->seconds += (unsigned)delta;
        app_data.seconds += (unsigned)delta;
        apps_dirty = 1;
        extra_track((unsigned)delta, 0, current >= 0 ? library[current].id : 0);
    }
    if (delta > 0 || ui_route() != 2 || sleeping)
        app_last_time = now;
}
static void apps_page_turned(void) {
    ReadingDay *d = app_day(app_date_key(time(NULL)), 1);
    d->pages++;
    app_data.pages++;
    extra_track(0, 1, current >= 0 ? library[current].id : 0);
    apps_dirty = 1;
}
static void apps_save(void) {
    if (!apps_dirty)
        return;
    char path[2048];
    snprintf(path, sizeof(path), "%s/apps-v1.bin", data_root);
    if (atomic_write(path, &app_data, sizeof(app_data)))
        apps_dirty = 0;
}
static void apps_pause(void) {
    apps_save();
    extra_save();
}
