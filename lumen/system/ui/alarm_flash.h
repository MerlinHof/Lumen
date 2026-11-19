static int v1_tick(void) {
    if (alarm_ring) {
        if (!timer_flash_started) {
            timer_flash_started = extra_millis();
            timer_last_phase = -1;
        }
        int64_t phase = (extra_millis() - timer_flash_started) / 500;
        if (phase < 120 && phase != timer_last_phase) {
            timer_last_phase = phase;
            return 1;
        }
    } else
        timer_flash_started = 0;
    return 0;
}
static int v1_poll(void) {
    if (alarm_ring && timer_flash_started && extra_millis() - timer_flash_started < 60000)
        return 500;
    return -1;
}
static void v1_effect(void) {
    if (alarm_ring && timer_flash_started && extra_millis() - timer_flash_started < 60000 &&
        (timer_last_phase & 1))
        for (size_t i = 0; i < sizeof(ui_pixels); i++)
            ui_pixels[i] = 255 - ui_pixels[i];
}
