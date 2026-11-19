static void extra_init(void) {
    extra_zone();
    char *last_time=service_read("time-last-success.txt",32);
    if(last_time){last_time_success=(time_t)atoll(last_time);free(last_time);}
    if(last_time_success<=time(NULL))last_sync=last_time_success;
    char stale[2048];
    local_path(stale, sizeof(stale), "service-private.tmp");
    unlink(stale);
    char ipath[2048];
    local_path(ipath, sizeof(ipath), "reading-insights-v1.bin");
    FILE *input = fopen(ipath, "rb");
    if (input) {
        Insights tmp;
        if (fread(&tmp, sizeof(tmp), 1, input) == 1 && tmp.magic == 0x494e5331)
            insights = tmp;
        fclose(input);
    }
    insights.magic = 0x494e5331;
    char b[1024] = "";
    extra_load_text("tools-settings.txt", b, sizeof(b));
    long long due = 0;
    sscanf(b, "%d %d %d %d %lld %d %d", &alarm_hour, &alarm_minute, &alarm_enabled, &timer_minutes,
           &due, &reading_goal, &legacy_clock_style);
    if (legacy_clock_style < 0 || legacy_clock_style > 13) legacy_clock_style = 0;
    if (alarm_hour < 0 || alarm_hour > 23)
        alarm_hour = 7;
    if (alarm_minute < 0 || alarm_minute > 59)
        alarm_minute = 0;
    if (timer_minutes < 1 || timer_minutes > 1440)
        timer_minutes = 20;
    if (reading_goal < 1 || reading_goal > 240)
        reading_goal = 20;
    if (due > time(NULL) && due < time(NULL) + 86400)
        timer_due = extra_millis() + (due - time(NULL)) * 1000;
    else if (due > time(NULL) - 86400 && due > 0)
        alarm_ring = 1;
    char pause_state[64] = "";
    extra_load_text("timer-paused.txt", pause_state, sizeof(pause_state));
    long long pause_ms = 0;
    sscanf(pause_state, "%lld", &pause_ms);
    if (!timer_due && pause_ms > 0 && pause_ms <= 86400000)
        timer_paused = pause_ms;

}
static void extra_save(void) {
    if (insights_dirty) {
        char p[2048];
        local_path(p, sizeof(p), "reading-insights-v1.bin");
        if (atomic_write(p, &insights, sizeof(insights)))
            insights_dirty = 0;
    }
    char b[256];
    long long due =
        timer_due ? (long long)time(NULL) + (timer_due - extra_millis() + 999) / 1000 : 0;
    snprintf(b, sizeof(b), "%d %d %d %d %lld %d %d\n", alarm_hour, alarm_minute, alarm_enabled,
             timer_minutes, due, reading_goal, legacy_clock_style);
    extra_write_text("tools-settings.txt", b);
    snprintf(b, sizeof(b), "%lld", (long long)timer_paused);
    extra_write_text("timer-paused.txt", b);
}
static int extra_ringing(void) {
    return alarm_ring;
}
static void extra_alarm_label(char *out, int cap) {
    out[0] = 0;
    if (alarm_enabled)
        snprintf(out, cap, "%02d:%02d", alarm_hour, alarm_minute);
}
static void extra_timer_label(char *out, int cap) {
    out[0] = 0;
    if (timer_due) {
        int seconds = (int)((timer_due - extra_millis() + 999) / 1000);
        if (seconds < 0)
            seconds = 0;
        snprintf(out, cap, "%d:%02d", seconds / 60, seconds % 60);
    }
}
static int extra_poll(void) {
    if (app_process_pending())return 25;
    if (release_state()==1) return 750;
    if (service_busy())
        return 250;
    if (timer_due || alarm_enabled)
        return 1000;
    int wait = -1;
    if (options[OPT_AUTO_TIME]) {
        long seconds = 86401 - (time(NULL) - last_sync);
        if (seconds < 1)
            seconds = 1;
        if (seconds > 86401)
            seconds = 86401;
        wait = (int)seconds * 1000;
    }
    if (options[OPT_CLOCK] && (active_app()!=document_provider() || reader_ui.reader_chrome)) {
        time_t now = time(NULL);
        struct tm t;
        localtime_r(&now, &t);
        int clock_wait = (60 - t.tm_sec) * 1000;
        if (wait < 0 || clock_wait < wait)
            wait = clock_wait;
    }
    return wait;
}
static int extra_tick(void) {
    int changed = v1_tick(), done = service_poll();
    release_poll();
    if (done) {
        changed = 1;
        if (done < 0) {
            if (done != -SERVICE_TIME || time(NULL)-last_time_success>172800)
                ui_message(service_error());
            if (done == -SERVICE_TIME)
                last_sync = time(NULL) - 82800;
        } else if (done == SERVICE_TIME) {
            last_sync = time(NULL);
            status_ui.status_time = 0;
            last_time_success=last_sync;
            char stamp[32],path[2048];snprintf(stamp,sizeof(stamp),"%lld",(long long)last_sync);
            local_path(path,sizeof(path),"time-last-success.txt");atomic_write(path,stamp,strlen(stamp));
            /* Routine successful sync is deliberately silent. */
        }
    }
    if (timer_due && extra_millis() >= timer_due) {
        timer_due = 0;
        alarm_ring = 1;
        pressed = -1;
        changed = 1;
        extra_save();
    }
    time_t now = time(NULL);
    struct tm t;
    localtime_r(&now, &t);
    if (alarm_enabled && t.tm_hour == alarm_hour && t.tm_min == alarm_minute &&
        now - alarm_last > 65) {
        alarm_enabled = 0;
        alarm_last = now;
        alarm_ring = 2;
        pressed = -1;
        changed = 1;
        extra_save();
    }
    if (options[OPT_AUTO_TIME] && !service_busy() && now - last_sync > 86400) {
        char ip[80];
        device_ip(ip, sizeof(ip));
        if (device_wifi_enabled() && *ip) {
            last_sync = now;
            service_start(SERVICE_TIME, "");
        } else
            last_sync = now - 86340;
    }
    static time_t hud_minute;
    if (options[OPT_CLOCK] && (active_app()!=document_provider() || reader_ui.reader_chrome) && now / 60 != hud_minute) {
        hud_minute = now / 60;
        changed = 1;
    }
    static int previous_minute = -1;
    int minute = timer_due ? (int)((timer_due - extra_millis() + 59999) / 60000) : -1;
    if (changed)
        return 1;
    if (timer_due || alarm_enabled) {
        if (minute != previous_minute) {
            previous_minute = minute;
            return active_app()==document_provider() && !reader_ui.reader_chrome ? 2 : 1;
        }
        return 2;
    }
    return 0;
}
