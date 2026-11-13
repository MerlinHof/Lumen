/* Persisted device idle-sleep policy. */
static int u16_ready, auto_sleep_minutes = 10;

static const int sleep_durations[] = {5, 10, 15, 30, 60, 0};
static void u16_load(void) {
    if (u16_ready)
        return;
    u16_ready = 1;
    activity16 = extra_millis();
    char *s = service_read("auto-sleep16.txt", 20);
    if (s) {
        char *end;
        long minutes=strtol(s,&end,10);
        if (end!=s && !*end)
            for (size_t i=0; i<sizeof(sleep_durations)/sizeof(*sleep_durations); i++)
                if (minutes==sleep_durations[i]) auto_sleep_minutes=(int)minutes;
        free(s);
    }
}
