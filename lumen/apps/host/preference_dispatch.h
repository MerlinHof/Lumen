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
