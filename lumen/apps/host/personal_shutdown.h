/* App-owned module; assembled by apps/host/compose.h. */
static void v1_close(void) {
    v1_save();

    free(photo_pixels);
    photo_pixels = NULL;
}

static void v1_timer_started(void) {
    timer_total_seconds = timer_minutes * 60;
    char b[32];
    snprintf(b, sizeof(b), "%d", timer_total_seconds);
    extra_write_text("timer-duration.txt", b);
}
static int v1_sleep_poll(void) {
    if (options[OPT_LOCK] != 4 || !photo_count)
        return -1;
    return (3601 - time(NULL) % 3600) * 1000;
}
static int v1_sleep_tick(void) {
    return options[OPT_LOCK] == 4 && photo_count && photo_hour_drawn != time(NULL) / 3600;
}
