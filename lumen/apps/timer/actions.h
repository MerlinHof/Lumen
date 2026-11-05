/* App-owned module; assembled by apps/host/compose.h. */
static void timer_action(int reset) {
    if (reset) {
        timer_due = timer_paused = 0;
    } else if (timer_due) {
        timer_paused = timer_due - extra_millis();
        if (timer_paused < 1)
            timer_paused = 1;
        timer_due = 0;
    } else {
        if (!timer_paused)
            v1_timer_started();
        timer_due = extra_millis() + (timer_paused ? timer_paused : (int64_t)timer_minutes * 60000);
        timer_paused = 0;
    }
}
