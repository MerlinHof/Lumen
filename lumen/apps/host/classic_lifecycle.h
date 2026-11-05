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
    apps_seed((uint32_t)time(NULL));
    time_t now = time(NULL);
    struct tm t;
    localtime_r(&now, &t);
    calendar_year = t.tm_year + 1900;
    calendar_month = t.tm_mon + 1;
    calendar_day = t.tm_mday;
}
static void apps_checkpoint(void) {
    int64_t now = app_millis(), delta = (now - app_last_time) / 1000;
    if (delta < 0)
        delta = 0;
    if (delta > 300)
        delta = 300;
    if (screen == 2 && !sleeping && delta > 0) {
        ReadingDay *d = app_day(app_date_key(time(NULL)), 1);
        d->seconds += (unsigned)delta;
        app_data.seconds += (unsigned)delta;
        apps_dirty = 1;
        extra_track((unsigned)delta, 0, current >= 0 ? library[current].id : 0);
    }
    if (delta > 0 || screen != 2 || sleeping)
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
    block_game.paused = snake_game.paused = 1;
    apps_save();
    extra_save();
}
static void app_records(void) {
    if ((unsigned)snake_game.score > app_data.best_snake) {
        app_data.best_snake = snake_game.score;
        apps_dirty = 1;
    }
    if ((unsigned)block_game.score > app_data.best_blocks) {
        app_data.best_blocks = block_game.score;
        apps_dirty = 1;
    }
    if (mine_game.won && !mine_recorded) {
        mine_recorded = 1;
        app_data.mines_wins++;
        apps_dirty = 1;
    }
    if (word_game.won && !word_recorded) {
        word_recorded = 1;
        app_data.word_wins++;
        apps_dirty = 1;
    }
}
static void app_new(int target) {
    if (target == 23) {
        mines_new(&mine_game);
        mine_flags = mine_release = mine_recorded = 0;
    }
    if (target == 24) {
        hangman_new(&word_game, options[OPT_LANGUAGE]);
        word_recorded = 0;
    }
    if (target == 25)
        blocks_new(&block_game);
    if (target == 26)
        snake_new(&snake_game);
    app_ready[target - 20] = 1;
    app_next_tick = app_millis() + 650;
}
static void app_open(int target) {
    route_to(target);
    if (target == 21)
        app_calendar_load();
    if (target >= 23 && !app_ready[target - 20])
        app_new(target);
}
static int launcher_page;
static const int launcher_targets[] = {27, 28, 29, 31, 32, 33, 21, 22, 38, 36, 23,
                                       24, 25, 26, 48, 49, 52, 63, 64, 65, 66, 67};
static void app_launcher(void) {
    v1_launcher();
}
