/* App-owned module; assembled by apps/host/compose.h. */
static void new_apps_init(void) {
    v1_init();
    pro_rng = (uint32_t)time(NULL) ^ (uint32_t)extra_millis();
    if (!app_state_load("focus-v1.bin", &focus_stats, sizeof(focus_stats)) || focus_stats.span < 2 ||
        focus_stats.span > 8 || focus_stats.level < 1 || focus_stats.level > 5) {
        memset(&focus_stats, 0, sizeof(focus_stats));
        focus_stats.span = 3;
        focus_stats.level = 1;
    }
    char *s = service_read("news-settings.txt", 80);
    if (s) {
        unsigned m = 63;
        int e = 0;
        if (sscanf(s, "%u %d", &m, &e) == 2) {
            news_topics = m & 63;
            news_enabled = e != 0;
        }
        free(s);
    }
    news_settings_loaded = 1;
    char *last = service_read("news/latest.txt", 32);
    if (last) {
        int date = atoi(last);
        if (date >= 20200101 && date <= 21001231)
            news_load(date);
        free(last);
    }
}
static void news_save_settings(void) {
    char b[64];
    snprintf(b, sizeof(b), "%u %d", news_topics, news_enabled);
    extra_write_text("news-settings.txt", b);
}
static void new_apps_save(void) {
    v1_save();
    if ((new_dirty & 2) && solitaire_ready &&
        app_state_save("solitaire-v1.bin", &solitaire, sizeof(solitaire)))
        new_dirty &= ~2u;
    if ((new_dirty & 8) && app_state_save("focus-v1.bin", &focus_stats, sizeof(focus_stats)))
        new_dirty &= ~8u;
}
static int valid_patience(void) {
    PatienceState *s = &solitaire.state;
    if (s->ns > 24 || s->nw > 24 || s->ns + s->nw > 24 || solitaire.undos < 0 ||
        solitaire.undos > 24)
        return 0;
    for (int i = 0; i < 7; i++) {
        if (s->count[i] > 52 || s->hidden[i] > s->count[i])
            return 0;
        for (int j = 0; j < s->count[i]; j++)
            if (s->column[i][j] >= 52)
                return 0;
    }
    for (int i = 0; i < 4; i++)
        if (s->foundation[i] > 13)
            return 0;
    return 1;
}
static void new_apps_open(int route) {
    if (route == 67) {
        polish_open(86);
        return;
    }
    if (route >= 70) {
        v1_open(route);
        return;
    }
    if (route == 64 && !solitaire_ready) {
        if (!app_state_load("solitaire-v1.bin", &solitaire, sizeof(solitaire)) || !valid_patience())
            patience_new(&solitaire, pro_random());
        solitaire_ready = 1;
        new_dirty |= 2;
    }
    if (route == 67) {
        news_enabled = 1;
        news_save_settings();
        news_page = 0;
        news_done = 0;
        news_puzzle_revealed = 0;
        char readname[80], readpath[2048];
        snprintf(readname, sizeof(readname), "news/%u-read.txt", newspaper.date);
        local_path(readpath, sizeof(readpath), readname);
        news_done = access(readpath, F_OK) == 0;
        if (!news_today_cached())
            news_retry = 0;
    }
}
static int new_apps_poll(void) {
    if (news_busy())
        return 250;

    if (news_enabled && !news_today_cached()) {
        long wait = news_retry - time(NULL);
        if (wait < 1)
            wait = 1;
        return wait > 60 ? 60000 : wait * 1000;
    }
    return -1;
}
static int new_apps_tick(void) {
    int olddate = newspaper.date, oldpage = news_page;
    int changed = 0, done = news_poll();
    if (done) {
        news_retry = time(NULL) + 3600;
        if (done > 0) {
            if (ui_route() == 67 && olddate && olddate != news_date()) {
                news_load(olddate);
                news_page = oldpage;
            } else
                news_page = 0;
            news_puzzle_revealed = 0;
            if (ui_route() == 67)
                changed = 1;
        } else if (ui_route() == 67) {
            ui_message("Neue Ausgabe nicht geladen. Gespeicherte Ausgabe bleibt verfügbar.");
            changed = 1;
        }
    }
    if (news_enabled && !news_busy() && !news_today_cached() && time(NULL) >= news_retry) {
        news_retry = time(NULL) + 60;
#ifndef LUMEN_HOST
        char ip[80];
        device_ip(ip, sizeof(ip));
        if (device_wifi_enabled() && *ip) {
            news_retry = time(NULL) + 3600;
            int before = newspaper.date;
            news_start(options[OPT_LANGUAGE], news_topics);
            if (ui_route() == 67 && before != (int)newspaper.date)
                changed = 1;
        }
#endif
    }

    return changed;
}
