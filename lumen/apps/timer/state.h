/* App-owned state; stable on-disk schemas are intentionally preserved. */
static int64_t timer_due,timer_paused;
static int alarm_hour=7,alarm_minute,alarm_enabled,alarm_ring,timer_minutes=20,timer_panel;
static time_t alarm_last;
