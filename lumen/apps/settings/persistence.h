/* App-owned module; assembled by apps/host/compose.h. */
/* LUMEN 1.6: small app surfaces share persistence, reader and background jobs. */
static int u16_ready, auto_sleep_minutes = 10;

static const char *sleep_choices16[] = {"5 min", "10 min", "15 min", "30 min", "60 min", "Nie"};
static const int sleep_minutes16[] = {5, 10, 15, 30, 60, 0};
static void u16_load(void) {
    if (u16_ready)
        return;
    u16_ready = 1;
    activity16 = extra_millis();
    char *s = service_read("auto-sleep16.txt", 20);
    if (s) {
        int n = atoi(s);
        for (int i = 0; i < 6; i++)
            if (n == sleep_minutes16[i])
                auto_sleep_minutes = n;
        free(s);
    }
}
