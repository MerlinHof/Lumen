/* App-owned module; assembled by apps/host/compose.h. */
static void u16_dropdown(int target, int choice) {
    if (target == 916) {
        reader_ui.reader_progress_mode = choice;
        options[OPT_PROGRESS] = choice != 5;
        library_options_save();
        char b[16];
        snprintf(b, 16, "%d", choice);
        extra_write_text("reader-progress15.txt", b);
    }
    if (target == 917 && choice >= 0 && choice < 6) {
        auto_sleep_minutes = sleep_minutes16[choice];
        char b[16];
        snprintf(b, 16, "%d", auto_sleep_minutes);
        extra_write_text("auto-sleep16.txt", b);
    }
}
static int u16_draw(void) {
    u16_load();

    return u16_apps_draw();
}
static int u16_tick(void) {
    u16_load();
    if (ink_dirty && !ink_drag && extra_millis() >= ink_save_at)
        ink_save();
    if (!sleeping && auto_sleep_minutes > 0 && !connect_running() &&
        extra_millis() - activity16 >= (int64_t)auto_sleep_minutes * 60000) {
        Input in = {.kind = INPUT_SLEEP};
        activity16 = extra_millis();
        device_request_sleep();
        return ui_input(&in);
    }
    return u16_jobs_tick();
}
static int u16_input(const Input *in) {
    u16_load();
    if (in->kind == INPUT_DOWN || in->kind == INPUT_TAP || in->kind == INPUT_SWIPE ||
        in->kind == INPUT_WAKE)
        activity16 = extra_millis();
    if (sleeping || editor_ui.keyboard || dropdown)
        return -1;
    return u16_apps_input(in);
}
