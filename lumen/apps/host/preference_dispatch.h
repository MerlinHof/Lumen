static int u16_tick(void) {
    u16_load();
    if (!sleeping && auto_sleep_minutes > 0 && !connect_running() &&
        extra_millis() - activity16 >= (int64_t)auto_sleep_minutes * 60000) {
        activity16 = extra_millis();
        device_request_sleep();
    }
    return u16_jobs_tick();
}
