/* App-owned module; assembled by apps/host/compose.h. */
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
    extra_load_text("ha-url.txt", ha_url, sizeof(ha_url));
    extra_load_text("ha-token.txt", ha_token, sizeof(ha_token));
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
    char *favorites = service_read("ha-favorites.txt", 22000);
    if (favorites) {
        char *save, *p = strtok_r(favorites, "\n", &save);
        while (p && ha_favorite_count < 128) {
            snprintf(ha_favorites[ha_favorite_count++], 160, "%s", p);
            p = strtok_r(NULL, "\n", &save);
        }
        free(favorites);
    }
    char wiki_path[2048];
    local_path(wiki_path, sizeof(wiki_path), "wiki-last.xml");
    if (!access(wiki_path, R_OK))
        discover_parse_lookup(wiki_path);
    extra_load_text("wiki-query.txt", wiki_query, sizeof(wiki_query));
    ha_parse();
    notes_scan();

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
static void storage_scan(void);
static void extra_open(int target) {
    if (history_count < 64 && (!history_count || history[history_count - 1] != ui_route()))
        history[history_count++] = ui_route();
    route_to(target);
    extra_prepare(target);
}
static void extra_prepare(int target) {
    if (target >= 60)
        new_apps_open(target);

    extra_page = 0;
    if (target == 40)
        storage_scan();
    if (target == 29)
        notes_scan();
    if (target == 36)
        wiki_image_requested[0] = 0;
    if (target == 36 && !discover_busy() && !*discover_lookup_text())
        extra_wiki_restore();



    if (target == 39) {
        time_t now = time(NULL);
        struct tm t;
        localtime_r(&now, &t);
        time_year = t.tm_year + 1900;
        time_month = t.tm_mon + 1;
        time_day = t.tm_mday;
        time_hour = t.tm_hour;
        time_minute = t.tm_min;
    }
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
    if (editor_ui.keyboard || ui_route() == 27)
        return 500;
    if (ui_route() == 36 && *discover_lookup_image() && !wiki_image_available() &&
        wiki_image_attempts < 3)
        return 1000;
    if (app_process_pending())return 25;
    if (release_state()==1) return 750;
    if (service_busy() || service_history_busy() || discover_busy())
        return 250;
    if (timer_due || alarm_enabled)
        return 1000;
    if (ui_route() == 28)
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
    if (options[OPT_CLOCK] && (ui_route() != 2 || reader_ui.reader_chrome)) {
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
    int changed = new_apps_tick() | v1_tick(), done = service_poll();
    if (release_poll() && ui_route()==40) changed=1;
    if ((done == SERVICE_WEATHER || done == -SERVICE_WEATHER) &&
        app_for_route(ui_route())==APP_WEATHER && app_package_installed(APP_WEATHER))
        app_process_send("{\"event\":\"refresh\"}");

    if ((editor_ui.keyboard || ui_route() == 27) && extra_millis() - editor_ui.cursor_epoch >= 600) {
        editor_ui.cursor_epoch = extra_millis();
        editor_ui.cursor_visible ^= 1;
        changed = 1;
    }
    if (ui_route() == 36 && !discover_busy() && !service_busy() && *discover_lookup_image() &&
        (strcmp(wiki_image_requested, discover_lookup_image()) ||
         (!wiki_image_available() && wiki_image_attempts < 3 &&
          time(NULL) >= wiki_image_retry_at))) {
        if (strcmp(wiki_image_requested, discover_lookup_image()))
            wiki_image_attempts = 0;
        snprintf(wiki_image_requested, sizeof(wiki_image_requested), "%s", discover_lookup_image());
        if (!wiki_image_available() &&
            service_start(SERVICE_WIKI_IMAGE, wiki_image_requested, "", "")) {
            wiki_image_attempts++;
            wiki_image_retry_at = time(NULL) + 30;
        }
    }
    if (done == SERVICE_WIKI_IMAGE)
        polish_wiki_saved();
    if (wiki_reader_pending &&
        (done == SERVICE_WIKI_IMAGE || done == -SERVICE_WIKI_IMAGE || !service_busy())) {
        wiki_reader_pending = 0;
        if (ui_route() == 36)
            article_wiki_open();
    }
    polish_wiki_archive();
    if (done) {
        changed = 1;
        if (done < 0) {
            if (done != -SERVICE_WIKI_IMAGE && done != -SERVICE_HA_HISTORY &&
                (done != -SERVICE_TIME || time(NULL)-last_time_success>172800))
                ui_message(service_error());
            if (done == -SERVICE_TIME)
                last_sync = time(NULL) - 82800;
        } else if (done == SERVICE_HA) {
            ha_parse();
            if (ha_refresh_at > time(NULL) + 5)
                ha_refresh_at = time(NULL) + 5;
        } else if (done == SERVICE_HA_HISTORY)
            ha_history_parse();
        else if (done == SERVICE_TOGGLE)
            service_start(SERVICE_HA, ha_url, ha_token, "");
        else if (done == SERVICE_TIME) {
            last_sync = time(NULL);
            status_ui.status_time = 0;
            last_time_success=last_sync;
            char stamp[32],path[2048];snprintf(stamp,sizeof(stamp),"%lld",(long long)last_sync);
            local_path(path,sizeof(path),"time-last-success.txt");atomic_write(path,stamp,strlen(stamp));
            /* Routine successful sync is deliberately silent. */
        }
    }
    if((done==SERVICE_HA || done==SERVICE_HA_HISTORY || done==SERVICE_TOGGLE ||
        done==-SERVICE_HA || done==-SERVICE_HA_HISTORY || done==-SERVICE_TOGGLE) &&
        app_for_route(ui_route())==APP_HOME_ASSISTANT)
        app_process_send("{\"event\":\"refresh\"}");
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
            service_start(SERVICE_TIME, "", "", "");
        } else
            last_sync = now - 86340;
    }
    static time_t hud_minute;
    if (options[OPT_CLOCK] && (ui_route() != 2 || reader_ui.reader_chrome) && now / 60 != hud_minute) {
        hud_minute = now / 60;
        changed = 1;
    }
    static int previous_minute = -1;
    int minute = timer_due ? (int)((timer_due - extra_millis() + 59999) / 60000) : -1;
    if (changed)
        return 1;
    if (timer_due || alarm_enabled) {
        if (ui_route() == 28 || minute != previous_minute) {
            previous_minute = minute;
            return ui_route() == 2 && !reader_ui.reader_chrome ? 2 : 1;
        }
        return 2;
    }
    if (ui_route() == 33 || ui_route() == 60)
        return 2;
    return 0;
}
static void extra_overlay(void) {
    if (!alarm_ring)
        return;
    widget_count = 0;
    rect(0, 0, 600, 800, 255);
    time_t now = time(NULL);
    struct tm t;
    localtime_r(&now, &t);
    char clock[40], date[64];
    strftime(clock, sizeof(clock), "%H:%M", &t);
    strftime(date, sizeof(date), "%d.%m.%Y", &t);
    centered(28, 72, 544, 90, clock, ui_font(UI_DISPLAY));
    centered(28, 166, 544, 40, date, ui_font(UI_BODY));
    centered(28, 240, 544, 70, alarm_ring == 1 ? "Zeit ist um" : "Wecker", ui_font(UI_TITLE));
    drawtext(42, 338, "Heute im Kalender", ui_font(UI_PRIMARY), 516);
    char name[80];
    snprintf(name, sizeof(name), "calendar/%04d%02d%02d.txt", t.tm_year + 1900, t.tm_mon + 1,
             t.tm_mday);
    char agenda_path[2048];
    snprintf(agenda_path,sizeof(agenda_path),"%s/../appdata/lumen.calendar/%s",
             data_root,name+strlen("calendar/"));
    FILE *day_note=fopen(agenda_path,"rb");
    char *agenda=NULL;
    if(day_note) {
        char content[512];
        size_t n=fread(content,1,sizeof(content)-1,day_note);
        fclose(day_note);
        content[n]=0;agenda=strdup(content);
    } else agenda=service_read(name,511);
    wrapped_raw(42, 390, agenda && *agenda ? agenda : tr("Keine Tagesnotiz"), ui_font(UI_BODY), 516, 5);
    free(agenda);
    button(80, 620, 440, "Weiterlesen", 0);
    if (pressed == 0)
        for (int y = 621; y < 667; y++)
            for (int x = 81; x < 519; x++)
                ui_pixels[y * 600 + x] = 255 - ui_pixels[y * 600 + x];
}
static int extra_intercept(const Input *in) {
    if (!alarm_ring)
        return -1;
    if (in->kind == INPUT_TAP && in->y >= 620 && in->y < 668) {
        alarm_ring = 0;
        pressed = -1;
        return 1;
    }
    if (in->kind == INPUT_DOWN) {
        pressed = hit(in->x, in->y);
        return 1;
    }
    if (in->kind == INPUT_TAP || in->kind == INPUT_MOVE || in->kind == INPUT_SWIPE ||
        in->kind == INPUT_HOLD)
        return 1;
    return -1;
}
/* Editable scientific expression with persistent answer/history. */
