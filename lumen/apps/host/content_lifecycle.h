/* App-owned module; assembled by apps/host/compose.h. */
static void polish_open(int route) {
    if (route == 72) {
        photo_fill = photo_rotation = 0;
        photo_controls = 1;
    }
    if (route == 94 || route == 36) {
        polish_wiki_archive();
        wiki_history_scan();
    }
    if (route == 86 || route == 67) {
        press_archive_scan();
        press_daily_enabled = 1;
        extra_write_text("press-daily-enabled.txt", "1");
        news_enabled = 0;
        news_save_settings();
        news_cancel();
    }
}
static void polish_view(void) {
    switch (ui_route()) {
    case 95:
    case 96:
    case 97:
        polish_news_draw();
        break;
    }
}
static int polish_input(const Input *in) {
    if (ui_route() == 95 || ui_route() == 96 || ui_route() == 97)
        return polish_news_input(in);
    return 2;
}
static int polish_tick(void) {
    int changed = 0, done = press_poll();
    if (done) {
        if (done > 0) {
            press_reader_hash = 0;
            if (ui_route() == 96 && press_loading_kind == 2) {
                article_news_open();
                changed = 1;
            }
            if (ui_route() == 86)
                press_archive_scan();
            if (ui_route() == 95 || ui_route() == 96 || ui_route() == 86)
                changed = 1;
        } else {
            if (ui_route() == 95 || ui_route() == 96) {
                ui_message("Nicht geladen. Gespeicherte Inhalte bleiben verfügbar.");
                changed = 1;
            }
            press_retry_at = time(NULL) + 3600;
        }
    }
    if (news_enabled) {
        news_enabled = 0;
        news_cancel();
        news_save_settings();
        press_daily_enabled = 1;
        extra_write_text("press-daily-enabled.txt", "1");
    }
    static int initialized;
    if (!initialized) {
        initialized = 1;
        char *s = service_read("press-daily-enabled.txt", 8);
        if (s) {
            press_daily_enabled = atoi(s) != 0;
            free(s);
        }
    }
    if (press_daily_enabled && !press_busy() && time(NULL) >= press_retry_at && ui_route() != 95 &&
        ui_route() != 96) {
        press_retry_at = time(NULL) + 60;
        char n[100], p[2048];
        snprintf(n, sizeof(n), "press/edition-%08d.bin", news_date());
        local_path(p, sizeof(p), n);
        if (access(p, R_OK)) {
            char ip[80];
            device_ip(ip, 80);
            if (device_wifi_enabled() && *ip) {
                press_retry_at = time(NULL) + 3600;
                press_start_day(news_date(), options[OPT_LANGUAGE]);
            }
        }
    }
    return changed;
}
static int polish_poll(void) {
    if (press_busy())
        return 250;
    if (press_daily_enabled)
        return 60000;
    return -1;
}
static int polish_commit(int target, const char *text) {
    if (target == 150) {
        if (!*text)
            return 1;
        snprintf(press_query, 256, "%s", text);
        press_cancel();
        memset(&press_list, 0, sizeof(press_list));
        snprintf(press_list.query, 256, "%s", text);
        press_list_page = 0;
        press_loading_kind = 1;
        press_search(text, options[OPT_LANGUAGE]);
        extra_open(95);
        return 1;
    }
    return 0;
}
static void polish_close(void) {
    press_cancel();
    polish_wiki_archive();
}

static void polish_wiki_refetched(void) {
    wiki_refresh_id = 0;
    char p[2048];
    wiki_cache_path(p, sizeof(p), wiki_key(), "deleted");
    unlink(p);
    wiki_saved_hash = 0;
    polish_wiki_archive();
}

static int wiki_refresh_failed(void) {
    if (!wiki_refresh_id)
        return 0;
    char path[2048];
    wiki_cache_path(path, sizeof(path), wiki_refresh_id, "bin");
    WikiSaved *a = malloc(sizeof(*a));
    FILE *f = fopen(path, "rb");
    int ok = a && f && fread(a, sizeof(*a), 1, f) == 1 && a->magic == 0x574b3131;
    if (f)
        fclose(f);
    if (ok) {
        a->title[255] = a->text[524287] = a->image[2047] = a->name[511] = 0;
        wiki_ui.wiki_article_language = a->language >= 0 && a->language < 4 ? a->language : 0;
        discover_restore_lookup(a->title, a->text, a->image, a->name);
        wiki_saved_hash = press_id(a->text) ^ wiki_key();
        wiki_cache_path(path, sizeof(path), wiki_refresh_id, "pgm");
        char target[2048];
        local_path(target, sizeof(target), "wiki-image.pgm");
        if (copy_file(path, target))
            extra_write_text("wiki-image-url.txt", a->image);
    }
    free(a);
    wiki_refresh_id = 0;
    return ok && article_wiki_open();
}

static void extra_wiki_cache(void) {
    char *xml = service_read("opds-feed.xml", 2097152);
    if (xml) {
        extra_write_text("wiki-last.xml", xml);
        free(xml);
    }
}
