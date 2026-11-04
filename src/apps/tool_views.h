/* LUMEN apps: persistent local tools and asynchronous network dashboards. */
#include <ctype.h>
#include <dirent.h>
#include <sys/time.h>
typedef struct {
    uint64_t id;
    unsigned seconds, pages, sessions;
} BookInsight;
typedef struct {
    unsigned magic, sessions;
    BookInsight books[BOOK_LIMIT];
} Insights;
static Insights insights;
static int insights_dirty;
static time_t insight_last;
static uint64_t insight_book;
static BookInsight *insight_get(uint64_t id, int create) {
    if (!id)
        return NULL;
    BookInsight *empty = NULL;
    for (int i = 0; i < BOOK_LIMIT; i++) {
        if (insights.books[i].id == id)
            return &insights.books[i];
        if (!insights.books[i].id && !empty)
            empty = &insights.books[i];
    }
    if (create && empty) {
        empty->id = id;
        return empty;
    }
    return NULL;
}
static void extra_track(unsigned seconds, unsigned pages, uint64_t id) {
    BookInsight *b = insight_get(id, 1);
    if (!b)
        return;
    time_t now = time(NULL);
    if (insight_book != id || now - insight_last > 600) {
        insights.sessions++;
        b->sessions++;
    }
    insight_book = id;
    insight_last = now;
    b->seconds += seconds;
    b->pages += pages;
    insights_dirty = 1;
}
static int extra_page, calc_degrees = 1, calc_alt, calc_cursor, calc_history_count,
                       calc_history_page, calc_history_mode;
static char calc_expr[1024], calc_result[128], calc_error[128], calc_history[16][1024];
static double calc_answer, calc_memory;
static int64_t extra_millis(void);
static int64_t timer_due, timer_paused;

static int alarm_hour = 7, alarm_minute, alarm_enabled, alarm_ring;
static time_t alarm_last;
static int timer_minutes = 20, timer_panel;
static int note_ids[1024], note_count, note_selected = -1, note_page;
static char note_text[8192];

#define DRAW_W 600
#define DRAW_H 688
static unsigned char drawing[DRAW_W * DRAW_H], drawing_undo[DRAW_W * DRAW_H];
static int drawing_count = 1;
static int drawing_ready, drawing_dirty, drawing_slot, drawing_eraser, drawing_width = 3,
                                                                       draw_x = -1, draw_y = -1;
static char weather_city[256] = "Rosenheim", ha_url[512], ha_token[512], wiki_query[256];
static char ha_favorites[128][160];
static int ha_favorite_count, ha_page, ha_select;
typedef struct {
    char id[160], name[192], state[64], unit[32];
    int sensor;
} HAEntity;
static HAEntity ha_entities[512];
static int ha_count;
static char wiki_image_requested[2048];
static time_t wiki_image_retry_at;
static int wiki_image_attempts;
static int wiki_image_available(void);
static int wiki_page, stats_page, reading_goal = 20;
static time_t last_sync;
static int weather_snapshot_valid;
static char *weather_snapshot;
static int time_year, time_month, time_day, time_hour, time_minute;
static const char *zone_names[] = {"Berlin / Europa", "UTC", "London", "New York", "Tokyo"};
static const char *zone_values[] = {"CET-1CEST,M3.5.0,M10.5.0/3", "UTC0",
                                    "GMT0BST,M3.5.0/1,M10.5.0", "EST5EDT,M3.2.0,M11.1.0", "JST-9"};
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
static int64_t extra_millis(void) {
    struct timespec t;
#ifndef LUMEN_HOST
    if (clock_gettime(CLOCK_BOOTTIME, &t))
        clock_gettime(CLOCK_MONOTONIC, &t);
#else
    clock_gettime(CLOCK_MONOTONIC, &t);
#endif
    return (int64_t)t.tv_sec * 1000 + t.tv_nsec / 1000000;
}
static void local_path(char *p, int cap, const char *suffix) {
    snprintf(p, cap, "%s/%s", data_root, suffix);
}
static void extra_write_text(const char *name, const char *s) {
    char p[2048];
    local_path(p, sizeof(p), name);
    if (!atomic_write(p, s, strlen(s)))
        ui_message("Speichern fehlgeschlagen.");
}
static void extra_load_text(const char *name, char *s, int cap) {
    char *p = service_read(name, cap - 1);
    if (p) {
        snprintf(s, cap, "%s", p);
        free(p);
    }
}
static void extra_zone(void) {
    int z = options[OPT_ZONE];
    if (z < 0 || z > 4)
        z = 0;
    setenv("TZ", zone_values[z], 1);
    tzset();
}
/* Small title cache, invalidated when notes are scanned after save, upload or
 * app activation. Scrolling/feedback redraws never reread whole note bodies. */
static unsigned note_list_generation;
static struct {
    int id;
    unsigned generation;
    char title[256];
} note_titles[14];
static unsigned note_title_slot;
static const char *note_list_title(int id) {
    for (int i = 0; i < 14; i++)
        if (note_titles[i].id == id && note_titles[i].generation == note_list_generation)
            return *note_titles[i].title ? note_titles[i].title : tr("Notiz");
    unsigned slot = note_title_slot++ % 14;
    note_titles[slot].id = id;
    note_titles[slot].generation = note_list_generation;
    char path[2048];
    snprintf(path, sizeof(path), "%s/notes/%d.txt", data_root, id);
    FILE *f = fopen(path, "r");
    note_titles[slot].title[0] = 0;
    if (f) {
        if (!fgets(note_titles[slot].title, sizeof(note_titles[slot].title), f))
            note_titles[slot].title[0] = 0;
        fclose(f);
    }
    note_titles[slot].title[strcspn(note_titles[slot].title, "\r\n")] = 0;
    return *note_titles[slot].title ? note_titles[slot].title : tr("Notiz");
}
static int note_newest_first(const void *a, const void *b) {
    int x = *(const int *)a, y = *(const int *)b;
    return (y > x) - (y < x);
}
static void notes_scan(void) {
    note_list_generation++;
    note_count = 0;
    char p[2048];
    local_path(p, sizeof(p), "notes");
    mkdir(p, 0700);
    DIR *d = opendir(p);
    if (!d)
        return;
    struct dirent *e;
    while ((e = readdir(d)) && note_count < 1024) {
        int id = 0, used = 0;
        if (sscanf(e->d_name, "%d.txt%n", &id, &used) == 1 && used > 0 && !e->d_name[used] &&
            id > 0) {
            char canonical[48];
            snprintf(canonical, sizeof(canonical), "%d.txt", id);
            if (!strcmp(canonical, e->d_name))
                note_ids[note_count++] = id;
        }
    }
    closedir(d);
    qsort(note_ids, note_count, sizeof(*note_ids), note_newest_first);
    if (note_page * 7 >= note_count)
        note_page = note_count ? (note_count - 1) / 7 : 0;
}
static void note_load(int id) {
    char name[80];
    snprintf(name, sizeof(name), "notes/%d.txt", id);
    note_text[0] = 0;
    extra_load_text(name, note_text, sizeof(note_text));
    note_selected = id;
}
static void ha_parse(void) {
    ha_count = 0;
    char *s = service_read("ha-states.json", 2 * 1024 * 1024);
    if (!s)
        return;
    JsonToken *tokens = calloc(32000, sizeof(*tokens));
    Json j;
    if (tokens && json_parse(&j, s, tokens, 32000)) {
        for (int n = 0; n < j.tokens[0].count && ha_count < 512; n++) {
            int obj = json_at(&j, 0, n);
            HAEntity v = {0};
            json_string(&j, json_get(&j, obj, "entity_id"), v.id, sizeof(v.id));
            if (strncmp(v.id, "light.", 6) && strncmp(v.id, "switch.", 7) &&
                strncmp(v.id, "input_boolean.", 14) && strncmp(v.id, "fan.", 4) &&
                strncmp(v.id, "sensor.", 7) && strncmp(v.id, "binary_sensor.", 14))
                continue;
            json_string(&j, json_get(&j, obj, "state"), v.state, sizeof(v.state));
            json_string(&j, json_get(&j, json_get(&j, obj, "attributes"), "friendly_name"), v.name,
                        sizeof(v.name));
            if (!*v.name)
                snprintf(v.name, sizeof(v.name), "%s", v.id);
            v.sensor = !strncmp(v.id, "sensor.", 7) || !strncmp(v.id, "binary_sensor.", 14);
            json_string(&j, json_get(&j, json_get(&j, obj, "attributes"), "unit_of_measurement"),
                        v.unit, sizeof(v.unit));
            ha_entities[ha_count++] = v;
        }
    }
    free(tokens);
    free(s);
}
static int ha_favorite(const char *id) {
    for (int i = 0; i < ha_favorite_count; i++)
        if (!strcmp(ha_favorites[i], id))
            return i;
    return -1;
}
#include "ha_widgets.h"
static void drawing_save(void) {
    if (!drawing_ready || !drawing_dirty)
        return;
    char name[80], p[2048];
    snprintf(name, sizeof(name), "drawing-%d.pgm", drawing_slot);
    local_path(p, sizeof(p), name);
    unsigned char *buf = malloc(sizeof(drawing) + 64);
    if (!buf)
        return;
    int n = snprintf((char *)buf, 64, "P5\n600 688\n255\n");
    memcpy(buf + n, drawing, sizeof(drawing));
    if (atomic_write(p, buf, n + sizeof(drawing)))
        drawing_dirty = 0;
    free(buf);
}
static void drawing_load(void) {
    char name[80], p[2048];
    snprintf(name, sizeof(name), "drawing-%d.pgm", drawing_slot);
    local_path(p, sizeof(p), name);
    memset(drawing, 255, sizeof(drawing));
    FILE *f = fopen(p, "rb");
    if (f) {
        char magic[3];
        int w, h, m;
        if (fscanf(f, "%2s %d %d %d", magic, &w, &h, &m) == 4 && !strcmp(magic, "P5") && m == 255) {
            fgetc(f);
            if (w == DRAW_W && h == DRAW_H) {
                if (fread(drawing, 1, sizeof(drawing), f) != sizeof(drawing))
                    memset(drawing, 255, sizeof(drawing));
            } else if (w == 520 && h == 510) {
                for (int row = 0; row < 510; row++)
                    if (fread(drawing + row * DRAW_W + 40, 1, 520, f) != 520) {
                        memset(drawing, 255, sizeof(drawing));
                        break;
                    }
            }
        }
        fclose(f);
    }
    memcpy(drawing_undo, drawing, sizeof(drawing));
    drawing_ready = 1;
    drawing_dirty = 0;
}
static void extra_init(void) {
    extra_zone();
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
    extra_load_text("weather-city.txt", weather_city, sizeof(weather_city));
    extra_load_text("ha-url.txt", ha_url, sizeof(ha_url));
    extra_load_text("ha-token.txt", ha_token, sizeof(ha_token));
    char calc_state[160] = "";
    extra_load_text("calculator-state.txt", calc_state, sizeof(calc_state));
    double a = 0, m = 0;
    int deg = 1;
    if (sscanf(calc_state, "%lf %lf %d", &a, &m, &deg) == 3 && isfinite(a) && isfinite(m)) {
        calc_answer = a;
        calc_memory = m;
        calc_degrees = !!deg;
    }
    char b[1024] = "";
    extra_load_text("tools-settings.txt", b, sizeof(b));
    long long due = 0;
    sscanf(b, "%d %d %d %d %lld %d", &alarm_hour, &alarm_minute, &alarm_enabled, &timer_minutes,
           &due, &reading_goal);
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
    char *history = service_read("calculator-history.txt", 17000);
    if (history) {
        char *save, *p = strtok_r(history, "\n", &save);
        while (p && calc_history_count < 16) {
            snprintf(calc_history[calc_history_count++], 1024, "%s", p);
            p = strtok_r(NULL, "\n", &save);
        }
        free(history);
    }
    char wiki_path[2048];
    local_path(wiki_path, sizeof(wiki_path), "wiki-last.xml");
    if (!access(wiki_path, R_OK))
        discover_parse_lookup(wiki_path);
    extra_load_text("wiki-query.txt", wiki_query, sizeof(wiki_query));
    ha_parse();
    notes_scan();
    DIR *drawdir = opendir(data_root);
    struct dirent *de;
    if (drawdir) {
        while ((de = readdir(drawdir))) {
            int id;
            char end;
            if (sscanf(de->d_name, "drawing-%d.pgm%c", &id, &end) == 1 && id >= 0 &&
                id < 2147483000 && id >= drawing_count)
                drawing_count = id + 1;
        }
        closedir(drawdir);
    }
}
static void extra_save(void) {
    drawing_save();
    char calc_state[160];
    snprintf(calc_state, sizeof(calc_state), "%.17g %.17g %d\n", calc_answer, calc_memory,
             calc_degrees);
    extra_write_text("calculator-state.txt", calc_state);
    if (insights_dirty) {
        char p[2048];
        local_path(p, sizeof(p), "reading-insights-v1.bin");
        if (atomic_write(p, &insights, sizeof(insights)))
            insights_dirty = 0;
    }
    char b[256];
    long long due =
        timer_due ? (long long)time(NULL) + (timer_due - extra_millis() + 999) / 1000 : 0;
    snprintf(b, sizeof(b), "%d %d %d %d %lld %d\n", alarm_hour, alarm_minute, alarm_enabled,
             timer_minutes, due, reading_goal);
    extra_write_text("tools-settings.txt", b);
    snprintf(b, sizeof(b), "%lld", (long long)timer_paused);
    extra_write_text("timer-paused.txt", b);
}
static void storage_scan(void);
static void extra_open(int target) {
    if (history_count < 64 && (!history_count || history[history_count - 1] != screen))
        history[history_count++] = screen;
    route_to(target);
    extra_prepare(target);
}
static void extra_prepare(int target) {
    if (target == 32)
        weather_snapshot_valid = 0;
    if (target >= 60)
        new_apps_open(target);
    if (target == 52)
        scratch_open();
    if (target == 33 || target == 34) {
        extra_load_text("ha-url.txt", ha_url, sizeof(ha_url));
        extra_load_text("ha-token.txt", ha_token, sizeof(ha_token));
        if (target == 33) {
            ha_parse();
            ha_history_parse();
            ha_history_key[0] = 0;
            ha_refresh_at = time(NULL) + 5;
        }
    }
    if (target == 48 || target == 49)
        games_init(target);
    extra_page = 0;
    if (target == 40)
        storage_scan();
    if (target == 29)
        notes_scan();
    if (target == 36)
        wiki_image_requested[0] = 0;
    if (target == 36 && !discover_busy() && !*discover_lookup_text())
        extra_wiki_restore();

#ifndef LUMEN_HOST
    if (target == 32 && !service_busy()) {
        char p[2048];
        local_path(p, sizeof(p), "weather.json");
        struct stat st;
        if (stat(p, &st) || time(NULL) - st.st_mtime > 1800)
            service_start(SERVICE_WEATHER, weather_city, "", "");
    }
    if (target == 33 && *ha_url && *ha_token && !service_busy())
        service_start(SERVICE_HA, ha_url, ha_token, "");
#endif
    if (target == 31 && !drawing_ready)
        drawing_load();

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
    if (keyboard || screen == 27 || *notification_text())
        return 500;
    if (screen == 36 && *discover_lookup_image() && !wiki_image_available() &&
        wiki_image_attempts < 3)
        return 1000;
    if (service_busy() || service_history_busy() || discover_busy())
        return 250;
    if (screen == 33 || screen == 60)
        return 500;
    if (timer_due || alarm_enabled)
        return 1000;
    if (screen == 28)
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
    if (options[OPT_CLOCK] && (screen != 2 || reader_chrome)) {
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
    int changed = new_apps_tick() | v1_tick() | v1_chess_tick(), done = service_poll();
    if (done == SERVICE_WEATHER || done == -SERVICE_WEATHER)
        weather_snapshot_valid = 0;
    changed |= notification_tick(extra_millis());

    if ((keyboard || screen == 27) && extra_millis() - cursor_epoch >= 600) {
        cursor_epoch = extra_millis();
        cursor_visible ^= 1;
        changed = 1;
    }
    if (screen == 36 && !discover_busy() && !service_busy() && *discover_lookup_image() &&
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
        if (screen == 36)
            article_wiki_open();
    }
    polish_wiki_archive();
    if (done) {
        changed = 1;
        if (done < 0) {
            if (done != -SERVICE_WIKI_IMAGE && done != -SERVICE_HA_HISTORY)
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
            status_time = 0;
            ui_message("Uhrzeit synchronisiert.");
        }
    }
    if ((screen == 33 || screen == 60) && !ha_select && !service_busy()) {
        if (ha_refresh_at > time(NULL) + 5)
            ha_refresh_at = time(NULL);
        if (time(NULL) >= ha_refresh_at && *ha_url && *ha_token) {
            ha_refresh_at = time(NULL) + 5;
            service_start(SERVICE_HA, ha_url, ha_token, "");
        } else
            ha_request_history();
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
            service_start(SERVICE_TIME, "", "", "");
        } else
            last_sync = now - 86340;
    }
    static time_t hud_minute;
    if (options[OPT_CLOCK] && (screen != 2 || reader_chrome) && now / 60 != hud_minute) {
        hud_minute = now / 60;
        changed = 1;
    }
    static int previous_minute = -1;
    int minute = timer_due ? (int)((timer_due - extra_millis() + 59999) / 60000) : -1;
    if (changed)
        return 1;
    if (timer_due || alarm_enabled) {
        if (screen == 28 || minute != previous_minute) {
            previous_minute = minute;
            return screen == 2 && !reader_chrome ? 2 : 1;
        }
        return 2;
    }
    if (screen == 33 || screen == 60)
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
    char *agenda = service_read(name, 511);
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
static void calc_preview(void) {
    calc_result[0] = 0;
    calc_error[0] = 0;
}
static void calc_insert(const char *s) {
    if (!text_insert(calc_expr, sizeof(calc_expr), &calc_cursor, s))
        return;
    calc_preview();
    calc_error[0] = 0;
}
static void calc_backspace(void) {
    text_backspace(calc_expr, &calc_cursor);
    calc_preview();
    calc_error[0] = 0;
}
static void calc_run(void) {
    double v;
    if (calc_evaluate(calc_expr, calc_degrees, calc_answer, calc_memory, &v, calc_error,
                      sizeof(calc_error))) {
        calc_answer = v;
        snprintf(calc_result, sizeof(calc_result), "%.*g", calc_precision16, v);
        if (!calc_history_count || strcmp(calc_history[calc_history_count - 1], calc_expr)) {
            if (calc_history_count == 16) {
                memmove(calc_history, calc_history + 1, 15 * sizeof(calc_history[0]));
                calc_history_count--;
            }
            snprintf(calc_history[calc_history_count++], sizeof(calc_history[0]), "%s", calc_expr);
        }
        char all[18000] = "";
        for (int i = 0; i < calc_history_count; i++) {
            strcat(all, calc_history[i]);
            strcat(all, "\n");
        }
        extra_write_text("calculator-history.txt", all);
    } else {
        calc_result[0] = 0;
        snprintf(calc_error, sizeof(calc_error), "%s", tr("Ausdruck oder Wertebereich prüfen."));
    }
}
static const char *calc_all0[] = {"sin", "cos", "tan", "log", "ln", "sqrt", "(",   ")",
                                  "pi",  "e",   "^",   "x",   "y",  "abs",  "ans", "%"};
static const char *calc_all1[] = {"asin",  "acos", "atan", "^2",  "^3",   "!",  "exp", "cbrt",
                                  "floor", "ceil", "1/",   "10^", "^0.5", "M+", "mem", "MC"};
static const char *calc_digits[] = {"7", "8", "9", "/", "4", "5", "6", "*",
                                    "1", "2", "3", "-", "0", ".", "%", "+"};
static const char *calc_label(const char *key) {
    if (!strcmp(key, "pi"))
        return "π";
    if (!strcmp(key, "sqrt"))
        return "√";
    if (!strcmp(key, "^2"))
        return "x²";
    if (!strcmp(key, "^3"))
        return "x³";
    if (!strcmp(key, "^0.5"))
        return "√x";
    if (!strcmp(key, "cbrt"))
        return "∛";
    if (!strcmp(key, "1/"))
        return "1/x";
    if (!strcmp(key, "10^"))
        return "10ˣ";
    if (!strcmp(key, "exp"))
        return "eˣ";
    if (!strcmp(key, "^"))
        return "xʸ";
    if (!strcmp(key, "*"))
        return "·";
    if (!strcmp(key, "/"))
        return "÷";
    if (!strcmp(key, "-"))
        return "−";
    return key;
}
static void calc_math(char *out, int cap) {
    const char *p = calc_expr;
    int n = 0;
    while (*p && n < cap - 8) {
        const char *replacement = NULL;
        int count = 1;
        if (!strncmp(p, "sqrt", 4)) {
            replacement = "√";
            count = 4;
        } else if (!strncmp(p, "pi", 2)) {
            replacement = "π";
            count = 2;
        } else if (!strncmp(p, "^2", 2) && !isdigit((unsigned char)p[2]) && p[2] != '.') {
            replacement = "²";
            count = 2;
        } else if (*p == '*')
            replacement = "·";
        else if (*p == '/')
            replacement = "÷";
        else if (*p == '-')
            replacement = "−";
        if (replacement) {
            int len = strlen(replacement);
            memcpy(out + n, replacement, len);
            n += len;
            p += count;
        } else
            out[n++] = *p++;
    }
    out[n] = 0;
}
#include "math_layout.h"
static void primary_key(int x, int y, int w, int h, const char *label) {
    action_button(x,y,w,h,label);
}
static void calc_key(int x, int y, int w, int h, const char *label) {
    region(x, y, w, h);
    key_outline(x, y, w, h);
    centered(x, y, w, h, label, ui_font(UI_PRIMARY));
}
static int calc_function_left(void) {
    return calc_alt ? 66 : 28;
}
static int calc_function_pitch(void) {
    return 63;
}
static void calc_function_key(int x, int y, const char *key) {
    region(x, y, 57, 44);
    key_outline(x, y, 57, 44);
    if (!strcmp(key, "1/")) {
        centered(x, y + 3, 57, 16, "1", ui_font(UI_SECONDARY));
        rect(x + 18, y + 21, 21, 2, 0);
        centered(x, y + 25, 57, 16, "x", ui_font(UI_SECONDARY));
        return;
    }
    const char *base = NULL, *power = NULL;
    if (!strcmp(key, "10^")) {
        base = "10";
        power = "x";
    } else if (!strcmp(key, "exp")) {
        base = "e";
        power = "x";
    } else if (!strcmp(key, "^2")) {
        base = "x";
        power = "2";
    } else if (!strcmp(key, "^3")) {
        base = "x";
        power = "3";
    } else if (!strcmp(key, "^")) {
        base = "x";
        power = "y";
    }
    if (base) {
        int bw = textwidth_raw(base, ui_font(UI_BODY)), pw = textwidth_raw(power, ui_font(UI_SECONDARY)),
            left = x + (57 - bw - pw - 2) / 2;
        label_at(left, y + 14, bw + 2, 24, base, ui_font(UI_BODY));
        label_at(left + bw + 2, y + 4, pw + 2, 20, power, ui_font(UI_SECONDARY));
        return;
    }
    centered(x, y, 57, 44, calc_label(key), ui_font(UI_BODY));
}
static int calc_input_start(void) {
    calc_cursor = text_boundary(calc_expr, calc_cursor < 0 ? 0 : (size_t)calc_cursor);
    return text_visible_start(calc_expr, calc_cursor, ui_font(UI_BODY), 468, 0);
}
static void app_calculator(void) {
    calc_alt = !!calc_alt;
    heading_actions("Rechner", 3);
    icon_button(408, 74, IC_GRAPH);
    icon_button(468, 74, IC_HISTORY);
    icon_button(528, 74, IC_SETTINGS);

    key_outline(28, 132, 544, 216);
    key_outline(28, 361, 498, 52);
    region(536, 365, 36, 44);
    icon(532, 365, IC_DELETE);
    int start = calc_input_start();
    char shown[1024];
    snprintf(shown, sizeof(shown), "%s", calc_expr + start);
    int ih, it = ink_top(*shown ? shown : "Ag", ui_font(UI_BODY), &ih);
    int input_y = 361 + (52 - ih) / 2 - it;
    drawtext_raw(43, input_y, shown, ui_font(UI_BODY), 468);
    char prefix[1024];
    snprintf(prefix, sizeof(prefix), "%.*s", calc_cursor - start, calc_expr + start);
    int cx = 43 + textwidth(prefix, ui_font(UI_BODY));
    if (cx < 516 && cursor_visible) {
        text_caret(cx, input_y, ui_font(UI_BODY));
    }
    if (calc_memory != 0)
        drawtext(525, 145, "M", ui_font(UI_SECONDARY), 30);
    math_formula(43, 140, 514, 150);
    if (*calc_error)
        wrapped(43, 285, calc_error, ui_font(UI_BODY), 510, 2);
    else {
        const char *value = *calc_result ? calc_result : "0";
        int w = textwidth(value, ui_font(UI_DISPLAY));
        drawtext_raw(w < 510 ? 557 - w : 43, 288, value, ui_font(UI_DISPLAY), 514);
    }
    const char **top = calc_alt ? calc_all1 : calc_all0;
    icon_button(calc_alt ? 20 : 536, 454, calc_alt ? IC_PREV : IC_NEXT);
    for (int i = 0; i < 16; i++)
        calc_function_key(calc_function_left() + (i % 8) * calc_function_pitch(),
                          429 + (i / 8) * 51, top[i]);
    for (int i = 0; i < 16; i++)
        calc_key(28 + (i % 4) * 108, 540 + (i / 4) * 62, 100, 56, calc_label(calc_digits[i]));
    calc_key(468, 540, 104, 56, "±");
    calc_key(468, 602, 104, 56, "AC");
    primary_key(468, 664, 104, 118, "=");
}
static void app_calc_history(void) {
    heading("Rechenverlauf");
    for (int r = 0; r < 8; r++) {
        int i = calc_history_count - 1 - (extra_page * 8 + r);
        if (i < 0)
            break;
        int y = 145 + r * 64;
        list_row(28, y, 544, 64, -1, 0, calc_history[i], NULL, ui_font(UI_BODY), 2);
    }
    pager(679, &extra_page, (calc_history_count + 7) / 8);
}
static void app_timer(void) {
    heading("Timer & Wecker");
    const char *tabs[] = {"Timer", "Wecker"};
    segments(28, 133, 544, 44, tabs, 2, timer_panel);
    char b[80];
    if (!timer_panel) {
        extra_timer_label(b, sizeof(b));
        if (!*b)
            snprintf(b, sizeof(b), "%02d:00", timer_minutes);
        v1_timer_ring(0);
        icon_button(528, 185, IC_FULLSCREEN);
        const int presets[] = {5, 10, 20, 30};
        for (int i = 0; i < 4; i++) {
            snprintf(b, sizeof(b), "%d min", presets[i]);
            button_h(28 + i * 140, 397, 124, 48, b, !timer_due && timer_minutes == presets[i]);
        }
        button_h(28, 472, 80, 52, "−", 0);
        snprintf(b, sizeof(b), "%d min", timer_minutes);
        button_h(122, 472, 356, 52, b, 0);
        button_h(492, 472, 80, 52, "+", 0);
        button_h(28, 566, 260, 64,
                 timer_due      ? "Pause"
                 : timer_paused ? "Fortsetzen"
                                : "Timer starten",
                 0);
        button_h(312, 566, 260, 64, "Zurücksetzen", 0);
    } else {
        centered(28, 225, 544, 36, "Weckzeit", ui_font(UI_SECONDARY));
        snprintf(b, sizeof(b), "%02d:%02d", alarm_hour, alarm_minute);
        button_h(100, 292, 400, 94, b, 0);
        label_at(30, 442, 430, 48, tr("Wecker aktiv"), ui_font(UI_PRIMARY));
        toggle(492, 450, alarm_enabled);
        wrapped(30, 535, "Einmaliger Wecker zur eingestellten Uhrzeit.", ui_font(UI_BODY), 540, 3);
    }
    wrapped(30, 664, "Visuelles Signal beim Lesen. Im Tiefschlaf kein Wecksignal.", ui_font(UI_SECONDARY), 540,
            3);
}
static void app_notes(void) {
    v1_notes_draw();
}
static const char *note_next_line(const char *p, int width) {
    const char *q = p, *space = NULL;
    int w = 0;
    while (*q && *q != '\n') {
        const char *old = q;
        unsigned c = glyph(&q);
        if (w + glyph_advance(ui_font(UI_BODY), c) > width) {
            q = space && space > p ? space : old;
            break;
        }
        w += glyph_advance(ui_font(UI_BODY), c);
        if (c == ' ')
            space = q;
    }
    if (q == p && *q && *q != '\n')
        glyph(&q);
    if (*q == '\n')
        q++;
    return q;
}
static void note_editor_draw(void) {
    u15_note_editor_draw();
}
static void note_editor_touch(int x, int y) {
    u15_note_editor_touch(x, y);
}
static void app_note(void) {
    if (!keyboard)
        v1_note_tools();
    if (!keyboard) {
        note_editor_draw();
        pager(748, &extra_page, u15_note_page_count());
        u15_note_overlay();
    }
}
static void app_drawing(void) {
    /* Canvas extends to all screen edges below one compact tool strip. */
    char b[40];
    snprintf(b, sizeof(b), "%d / %d", drawing_slot + 1, drawing_count);
    button_h(8, 57, 90, 46, b, 0);
    if (drawing_count > 1) {
        page_arrow(100, 58, IC_PREV, drawing_slot > 0);
        page_arrow(148, 58, IC_NEXT, drawing_slot + 1 < drawing_count);
    }
    icon_button(202, 58, IC_PLUS);
    button_h(257, 57, 112, 46, drawing_eraser ? "Radierer" : "Stift", drawing_eraser);
    region(384, 58, 44, 44);
    app_icon(384, 58, APP_PHOTOS);
    icon_button(450, 58, IC_UNDO);
    icon_button(516, 58, IC_TRASH);
    for (int y = 0; y < DRAW_H; y++)
        memcpy(ui_pixels + (y + 112) * 600, drawing + y * DRAW_W, DRAW_W);
}
static void draw_line(int x, int y) {
    y -= 112;
    if (x < 0 || x >= DRAW_W || y < 0 || y >= DRAW_H) {
        draw_x = draw_y = -1;
        return;
    }
    if (draw_x < 0) {
        draw_x = x;
        draw_y = y;
    }
    int dx = abs(x - draw_x), sx = draw_x < x ? 1 : -1, dy = -abs(y - draw_y),
        sy = draw_y < y ? 1 : -1, e = dx + dy;
    for (;;) {
        float actual_radius = drawing_eraser ? 12 : drawing_width / 2.f;
        int radius = (int)ceilf(actual_radius);
        for (int yy = -radius; yy <= radius; yy++)
            for (int xx = -radius; xx <= radius; xx++) {
                int px = draw_x + xx, py = draw_y + yy;
                float a = actual_radius + 0.5f - sqrtf(xx * xx + yy * yy);
                if (a > 0 && px >= 0 && px < DRAW_W && py >= 0 && py < DRAW_H) {
                    if (a > 1)
                        a = 1;
                    int old = drawing[py * DRAW_W + px];
                    drawing[py * DRAW_W + px] =
                        (unsigned char)(old + ((drawing_eraser ? 255 : 0) - old) * a);
                }
            }
        if (draw_x == x && draw_y == y)
            break;
        int e2 = 2 * e;
        if (e2 >= dy) {
            e += dy;
            draw_x += sx;
        }
        if (e2 <= dx) {
            e += dx;
            draw_y += sy;
        }
    }
    drawing_dirty = 1;
}
static void weather_symbol(int x, int y, int code, int size) {
    int c = x + size / 2, d = y + size / 2, t = size > 90 ? 3 : 2;
    if (code == -1) {
        aa_disc(c, d, size / 4, 0);
        aa_disc(c + size / 9, d - size / 10, size / 4, 255);
        return;
    }
    int sun = code <= 2;
    if (sun) {
        int sx = code == 2 ? x + size / 3 : c, sy = code == 2 ? y + size / 3 : d;
        ring(sx, sy, size / 7, t);
        for (int i = 0; i < 8; i++) {
            float a = i * 3.14159265f / 4;
            stroke(sx + cosf(a) * size / 4, sy + sinf(a) * size / 4, sx + cosf(a) * size / 3,
                   sy + sinf(a) * size / 3, t);
        }
    }
    if (code >= 2) {
        int cy = y + size * 3 / 5;
        /* Single outline union, no seams between cloud lobes. */
        for (int yy = y; yy <= y + size; yy++)
            for (int xx = x; xx <= x + size; xx++) {
                float a = hypotf(xx - (x + size * .34f), yy - (y + size * .54f)) - size * .15f;
                float b = hypotf(xx - (x + size * .52f), yy - (y + size * .43f)) - size * .21f;
                float e = hypotf(xx - (x + size * .73f), yy - (y + size * .56f)) - size * .14f;
                float box = fmaxf(fabsf(xx - (x + size * .53f)) - size * .24f,
                                  fabsf(yy - (y + size * .61f)) - size * .09f);
                float dist = fminf(fminf(a, b), fminf(e, box));
                if (dist <= .5f)
                    blend_pixel(xx, yy, .5f - dist, 255);
                if (dist >= -t)
                    blend_pixel(xx, yy, fminf(.5f - dist, dist + t + .5f), 0);
            }
        (void)cy;
    }
    if (code == 45 || code == 48) {
        stroke(x + size / 4, y + size * 4 / 5, x + size * 4 / 5, y + size * 4 / 5, t);
        stroke(x + size / 3, y + size * 9 / 10, x + size * 2 / 3, y + size * 9 / 10, t);
    } else if (code >= 95) {
        stroke(c, y + size * 3 / 4, c - size / 10, y + size * 7 / 8, t + 1);
        stroke(c - size / 10, y + size * 7 / 8, c + size / 10, y + size * 7 / 8, t + 1);
        stroke(c + size / 10, y + size * 7 / 8, c, y + size, t + 1);
    } else if (code >= 50) {
        for (int i = 0; i < 3; i++) {
            int px = x + size / 3 + i * size / 6, py = y + size * 4 / 5;
            if ((code >= 70 && code <= 79) || code == 85 || code == 86) {
                stroke(px - 3, py, px + 3, py, t);
                stroke(px, py - 3, px, py + 3, t);
            } else
                stroke(px, py, px - size / 25, py + size / 12, t);
        }
    }
}

static void app_weather(void) {
    heading_actions("Wetter", 2);
    icon_button(468, 74, IC_SEARCH);
    icon_button(528, 74, IC_RESTART);
    static JsonToken tokens[4096];
    static Json j;
    static char location[256];
    static int valid;
    if (!weather_snapshot_valid) {
        free(weather_snapshot);
        weather_snapshot = service_read("weather.json", 100000);
        valid = weather_snapshot && json_parse(&j, weather_snapshot, tokens, 4096);
        char *place = service_read("weather-place.json", 65536);
        JsonToken geo_tokens[2048];
        Json geo;
        location[0] = 0;
        if (place && json_parse(&geo, place, geo_tokens, 2048)) {
            int first = json_at(&geo, json_get(&geo, 0, "results"), 0);
            json_string(&geo, json_get(&geo, first, "name"), location, sizeof(location));
        }
        free(place);
        weather_snapshot_valid = 1;
    }
    label_at(30, 128, 540, 40, *location ? location : weather_city, ui_font(UI_PRIMARY));
    if (!valid) {
        wrapped(30, 210,
                service_busy() ? "Wetter wird geladen …"
                               : "Ort suchen oder Vorhersage aktualisieren.",
                ui_font(UI_TITLE), 540, 4);
        return;
    }
    int current = json_get(&j, 0, "current"), daily = json_get(&j, 0, "daily");
    double temp = json_number(&j, json_get(&j, current, "temperature_2m")),
           feel = json_number(&j, json_get(&j, current, "apparent_temperature")),
           wind = json_number(&j, json_get(&j, current, "wind_speed_10m")),
           code = json_number(&j, json_get(&j, current, "weather_code"));
    if (!isfinite(temp) || !isfinite(code)) {
        return;
    }
    double is_day = json_number(&j, json_get(&j, current, "is_day"));
    weather_symbol(30, 180, is_day == 0 && code <= 1 ? -1 : (int)code, 130);
    char b[120];
    snprintf(b, sizeof(b), "%.0f °C", temp);
    drawtext(200, 194, b, ui_font(UI_DISPLAY), 370);
    snprintf(b, sizeof(b), "%s %.0f°  ·  %.0f km/h", tr("Gefühlt"), feel, wind);
    drawtext(200, 265, b, ui_font(UI_BODY), 370);
    int rises = json_get(&j, daily, "sunrise"), sets = json_get(&j, daily, "sunset"),
        daylight = json_get(&j, daily, "daylight_duration");
    char rise[64], set[64];
    json_string(&j, json_at(&j, rises, 0), rise, sizeof(rise));
    json_string(&j, json_at(&j, sets, 0), set, sizeof(set));
    double light = json_number(&j, json_at(&j, daylight, 0));
    snprintf(b, sizeof(b), "%s %.5s  ·  %s %.5s", tr("Aufgang"),
             strlen(rise) > 11 ? rise + 11 : "--:--", tr("Untergang"),
             strlen(set) > 11 ? set + 11 : "--:--");
    drawtext(30, 330, b, ui_font(UI_BODY), 540);
    snprintf(b, sizeof(b), "%s %.0f h %02.0f min", tr("Tageslicht"), floor(light / 3600),
             floor(fmod(light, 3600) / 60));
    drawtext(30, 368, b, ui_font(UI_SECONDARY), 540);
    int dates = json_get(&j, daily, "time"), codes = json_get(&j, daily, "weather_code"),
        high = json_get(&j, daily, "temperature_2m_max"),
        low = json_get(&j, daily, "temperature_2m_min"),
        rain = json_get(&j, daily, "precipitation_probability_max");
    for (int i = 0; i < 5; i++) {
        int x = 28 + i * 112;
        char date[64];
        json_string(&j, json_at(&j, dates, i), date, sizeof(date));
        snprintf(b, sizeof(b), "%.2s.%.2s.", strlen(date) > 8 ? date + 8 : "--",
                 strlen(date) > 5 ? date + 5 : "--");
        centered(x, 437, 100, 34, b, ui_font(UI_SECONDARY));
        double c = json_number(&j, json_at(&j, codes, i));
        weather_symbol(x + 12, 478, isfinite(c) ? (int)c : 3, 80);
        snprintf(b, sizeof(b), "%.0f / %.0f°", json_number(&j, json_at(&j, high, i)),
                 json_number(&j, json_at(&j, low, i)));
        centered(x, 579, 100, 36, b, ui_font(UI_BODY));
        snprintf(b, sizeof(b), "%.0f%%", json_number(&j, json_at(&j, rain, i)));
        centered(x, 623, 100, 32, b, ui_font(UI_SECONDARY));
    }
    char stamp[64];
    json_string(&j, json_get(&j, current, "time"), stamp, sizeof(stamp));
    snprintf(b, sizeof(b), "Open-Meteo · %.16s%s", stamp, service_busy() ? " …" : "");
    drawtext(30, 697, b, ui_font(UI_SECONDARY), 540);
}
static void app_home_assistant(void) {
    ha_dashboard();
}
static void app_ha_setup(void) {
    heading("Home Assistant");
    drawtext(30, 142, "Serveradresse", ui_font(UI_BODY), 540);
    button_h(28, 188, 544, 52, *ha_url ? ha_url : "http://homeassistant.local:8123", 0);
    drawtext(30, 281, "Langlebiger Zugriffstoken", ui_font(UI_BODY), 540);
    button_h(28, 327, 544, 52, *ha_token ? "••••••••  ·  Ändern" : "Token eingeben", 0);
    wrapped(30, 414,
            "Am Handy: Bücher übertragen starten, die Webseite öffnen und Home Assistant wählen. "
            "Adresse und Token dort einfügen. Danach diese App erneut öffnen.",
            ui_font(UI_BODY), 540, 5);
    button(28, 607, 544, "Verbinden", 0);
}
static int wiki_offsets[1024], wiki_pages;
static uint64_t wiki_hash;
static int wiki_image_available(void) {
    if (!*discover_lookup_image())
        return 0;
    char *key = service_read("wiki-image-url.txt", 2047);
    int same = key && !strcmp(key, discover_lookup_image());
    free(key);
    char p[2048];
    struct stat st;
    local_path(p, sizeof(p), "wiki-image.pgm");
    return same && !stat(p, &st) && st.st_size >= 130000;
}
static void app_wiki(void) {
    wiki_history_current();
    polish_wiki_history();
    if (discover_busy() || wiki_reader_pending) {
        rect(80, 235, 440, 180, 255);
        key_outline(80, 235, 440, 180);
        aa_arc(300, 285, 17, 35, 310, 4);
        centered(95, 325, 410, 60, "Artikel wird geladen …", ui_font(UI_PRIMARY));
    }
}

typedef struct {
    int book, index;
    char text[2048];
    ReaderPosition position;
} GlobalQuote;
static int quote_counts[BOOK_LIMIT], global_quote_count, global_quote_page;
static void quotes_load(void) {
    global_quote_count = 0;
    memset(quote_counts, 0, sizeof(quote_counts));
    for (int b = 0; b < library_count; b++) {
        char path[2048];
        snprintf(path, sizeof(path), "%s/books/%016llx.notes-v1", data_root,
                 (unsigned long long)library[b].id);
        struct stat st;
        if (!stat(path, &st) && st.st_size > 0) {
            int count = (int)(st.st_size / sizeof(Annotation));
            if (count > 128)
                count = 128;
            quote_counts[b] = count;
            global_quote_count += count;
        }
    }
    if (global_quote_page * 4 >= global_quote_count)
        global_quote_page = 0;
}
static int quote_get(int index, GlobalQuote *q) {
    if (index < 0)
        return 0;
    for (int b = 0; b < library_count; b++) {
        if (index >= quote_counts[b]) {
            index -= quote_counts[b];
            continue;
        }
        char path[2048];
        snprintf(path, sizeof(path), "%s/books/%016llx.notes-v1", data_root,
                 (unsigned long long)library[b].id);
        FILE *f = fopen(path, "rb");
        if (!f)
            return 0;
        Annotation a;
        int ok = !fseek(f, (long)index * sizeof(a), SEEK_SET) && fread(&a, sizeof(a), 1, f) == 1;
        fclose(f);
        if (!ok)
            return 0;
        a.text[sizeof(a.text) - 1] = 0;
        q->book = b;
        q->index = index;
        q->position = a.position;
        snprintf(q->text, sizeof(q->text), "%s", a.text);
        return 1;
    }
    return 0;
}
static int quote_hit_top[4], quote_hit_bottom[4], quote_hit_count;
static void app_quotes(void) {
    heading("Zitate");
    quote_hit_count = 0;
    int y = 146;
    for (int r = 0; r < 4 && global_quote_page * 4 + r < global_quote_count; r++) {
        GlobalQuote row, *q = &row;
        if (!quote_get(global_quote_page * 4 + r, q))
            continue;

        const char *p = q->text;
        int lines = 0;
        while (*p && lines < 3) {
            p = note_next_line(p, 532);
            lines++;
        }
        wrapped_raw(34, y, q->text, ui_font(UI_BODY), 532, 3);
        drawtext_raw(34, y + lines * (small.h - 4) + 5, library[q->book].title, ui_font(UI_SECONDARY), 532);
        int bottom = y + lines * (small.h - 4) + 34;
        region(28, y, 544, bottom - y);
        quote_hit_top[quote_hit_count] = y;
        quote_hit_bottom[quote_hit_count++] = bottom;
        divider(28, bottom, 544);
        y = bottom + 16;
    }
    if (!global_quote_count)
        wrapped(30, 180, "Alle markierten Zitate deiner Bücher erscheinen hier.", ui_font(UI_TITLE), 540,
                4);
    pager(679, &global_quote_page, (global_quote_count + 3) / 4);
}
static void app_time_settings(void) {
    heading("Datum & Uhrzeit");
    char b[80];
    time_t now = time(NULL);
    struct tm t;
    localtime_r(&now, &t);
    strftime(b, sizeof(b), "%d.%m.%Y  %H:%M", &t);
    key_outline(28, 140, 544, 108);
    centered(28, 140, 544, 108, b, ui_font(UI_TITLE));
    label_at(30, 290, 430, 48, tr("Automatisch über WLAN"), ui_font(UI_BODY));
    toggle(492, 298, options[OPT_AUTO_TIME]);
    if (options[OPT_AUTO_TIME]) {
        wrapped(30, 385, "Datum und Uhrzeit werden täglich über WLAN abgeglichen.", ui_font(UI_BODY), 540, 3);

    } else {
        button_h(28, 387, 544, 54, "Datum und Uhrzeit setzen", 0);
        label_at(30, 480, 220, 52, tr("Zeitzone"), ui_font(UI_BODY));
        dropdown_button(270, 480, 302, 52, zone_names[options[OPT_ZONE] % 5]);
    }
}
static uint64_t directory_bytes(const char *p, int depth) {
    if (depth > 12)
        return 0;
    struct stat st;
    if (lstat(p, &st))
        return 0;
    if (S_ISLNK(st.st_mode))
        return 0;
    if (S_ISREG(st.st_mode))
        return (uint64_t)st.st_size;
    if (!S_ISDIR(st.st_mode))
        return 0;
    DIR *d = opendir(p);
    if (!d)
        return 0;
    uint64_t n = 0;
    struct dirent *e;
    while ((e = readdir(d))) {
        if (e->d_name[0] == '.')
            continue;
        char child[2048];
        if (snprintf(child, sizeof(child), "%s/%s", p, e->d_name) >= (int)sizeof(child))
            continue;
        n += directory_bytes(child, depth + 1);
    }
    closedir(d);
    return n;
}
static uint64_t storage_books, storage_system, storage_free, storage_total, storage_firmware;
static void storage_scan(void) {
#ifndef LUMEN_HOST
    struct statvfs firmware;
    if (!statvfs("/", &firmware))
        storage_firmware = (uint64_t)(firmware.f_blocks - firmware.f_bfree) * firmware.f_frsize;
#endif
    struct statvfs v;
    if (!statvfs(data_root, &v)) {
        storage_total = (uint64_t)v.f_blocks * v.f_frsize;
        storage_free = (uint64_t)v.f_bavail * v.f_frsize;
    }
    storage_books = directory_bytes(library_root, 0);
    char root[2048];
    snprintf(root, sizeof(root), "%s/..", data_root);
    storage_system = directory_bytes(root, 0);
}
static void storage_size(char *out, int cap, uint64_t n) {
    snprintf(out, cap, "%.1f %s", n >= 1073741824 ? n / 1073741824.0 : n / 1048576.0,
             n >= 1073741824 ? "GB" : "MB");
}
static void system_screen(void) {
    heading("System");
    const char *tabs[] = {"Speicher & Version", "Verwaltung"};
    segments(28, 133, 544, 44, tabs, 2, extra_page);
    if (!extra_page) {
        drawtext(30, 205, "LUMEN 2.0", ui_font(UI_TITLE), 540);
        drawtext(30, 253, "Kindle KT2 · 600 × 800", ui_font(UI_SECONDARY), 540);
        wrapped(30, 289,
                "Diese Software wurde Ende 2026 von GPT-6.1 Sol entwickelt unter Aufsicht und "
                "Prompting und Lizenz von Merlin Hof.",
                ui_font(UI_SECONDARY), 540, 3);
        uint64_t total = storage_total, free = storage_free, books = storage_books,
                 lumen = storage_system;
        if (free > total)
            free = total;
        if (books > total - free)
            books = total - free;
        if (lumen > total - free - books)
            lumen = total - free - books;
        uint64_t sizes[] = {books, lumen, storage_firmware, total - free - books - lumen, free};
        total += storage_firmware;
        const char *labels[] = {"Bücher", "LUMEN & Daten", "Firmware (Systempartition)", "Andere",
                                "Frei"};
        int colors[] = {0, 72, 120, 175, 255};
        char b[100], size[40];
        storage_size(size, sizeof(size), total);
        snprintf(b, sizeof(b), "%s %s", size, tr("Gesamt"));
        drawtext(30, 383, b, ui_font(UI_PRIMARY), 540);
        rect(28, 438, 544, 28, 255);
        int x = 28;
        for (int i = 0; i < 5; i++) {
            int w = total ? (int)(544.0 * sizes[i] / total) : 0;
            if (i == 4)
                w = 572 - x;
            rect(x, 438, w, 28, colors[i]);
            if (i && w > 0)
                rect(x, 438, 2, 28, 255);
            x += w;
        }
        key_outline(28, 438, 544, 28);
        for (int i = 0; i < 5; i++) {
            int xx = 30 + (i % 2) * 282, y = 486 + (i / 2) * 67;
            rect(xx, y + 7, 15, 15, colors[i]);
            if (i == 4)
                key_outline(xx, y + 7, 15, 15);
            label_at(xx + 28, y, 230, 28, tr(labels[i]), i == 2 ? ui_font(UI_SECONDARY) : ui_font(UI_BODY));
            storage_size(b, sizeof(b), sizes[i]);
            label_at(xx + 28, y + 29, 230, 22, b, ui_font(UI_SECONDARY));
        }
        wrapped(30, 695, "USB-Speicher und belegte Systempartition.", ui_font(UI_SECONDARY), 540, 1);
    } else {
        label_at(30, 216, 430, 48, tr("LUMEN nach dem Booten"), ui_font(UI_BODY));
        toggle(492, 224, options[OPT_AUTOSTART]);
        const char *actions[] = {"Automatischer Ruhezustand", "Konfiguration exportieren",
                                 "Konfiguration importieren", "Amazon öffnen",
                                 "Einstellungen zurücksetzen"};
        for (int i = 0; i < 5; i++) {
            if (!i)
                u17_sleep_row(304);
            else
                button_h(28, 304 + i * 84, 544, 60, actions[i], 0);
        }
    }
}
static int system_input(const Input *in) {
    if (in->kind != INPUT_TAP)
        return 2;
    int x = in->x, y = in->y;
    if (y >= 133 && y < 177) {
        extra_page = x >= 300;
        if (!extra_page)
            storage_scan();
        return 1;
    }
    if (!extra_page)
        return 1;
    if (y >= 210 && y < 270) {
        options[OPT_AUTOSTART] ^= 1;
        char p[2048];
        local_path(p, sizeof(p), "../DISABLE_AUTOSTART");
        if (options[OPT_AUTOSTART]) {
#ifndef LUMEN_HOST
            if (access("/etc/upstart/lumen.conf", F_OK) != 0 &&
                system("/bin/sh /mnt/us/lumen/install-autostart.sh") != 0) {
                options[OPT_AUTOSTART] = 0;
                ui_message("Speichern fehlgeschlagen.");
            }
#endif
            if (options[OPT_AUTOSTART])
                unlink(p);
        } else
            atomic_write(p, "", 0);
        library_settings_save();
        return 1;
    }
    if (y >= 304 && y < 700) {
        int i = (y - 304) / 84;
        char p[2048];
        if (i == 0)
            u17_sleep_select(304);
        if (i == 1) {
            local_path(p, sizeof(p), "../LUMEN-Einstellungen.txt");
            ui_message(library_config_export(p) ? "Export im Ordner lumen gespeichert."
                                                : "Export fehlgeschlagen.");
        }
        if (i == 2) {
            local_path(p, sizeof(p), "../LUMEN-Einstellungen.txt");
            ui_message(library_config_import(p)
                           ? "Einstellungen importiert."
                           : "Importdatei fehlt oder enthält ungültige Werte.");
        }
        if (i == 3)
            wifi_exit = 1;
        if (i == 4)
            extra_open(41);
    }
    return 1;
}
static void app_reset(void) {
    heading("Zurücksetzen");
    wrapped(30, 172,
            "Alle Einstellungen auf Standard zurücksetzen? Bücher, Lesestände, Notizen, "
            "Zeichnungen und Zitate bleiben erhalten.",
            ui_font(UI_TITLE), 540, 5);
    button(28, 455, 544, "Einstellungen zurücksetzen", 0);
    button(28, 530, 544, "Abbrechen", 0);
}
static void extra_draw(void) {
    switch (screen) {
    case 51:
        heading("Bildquelle");
        wrapped_raw(30, 150, discover_lookup_image_name(), ui_font(UI_PRIMARY), 540, 5);
        wrapped(30, 330,
                "Freies Artikelbild von Wikimedia. Die genaue Lizenz und der Urheber stehen auf "
                "der Dateiseite:",
                ui_font(UI_BODY), 540, 4);
        char source[1024];
        snprintf(source, sizeof(source), "https://commons.wikimedia.org/wiki/File:%s",
                 discover_lookup_image_name());
        wrapped_raw(30, 470, source, ui_font(UI_SECONDARY), 540, 10);
        break;
    case 48:
        app_chess();
        break;
    case 49:
        app_tic();
        break;
    case 50:
        heading("Neue Partie?");
        wrapped(30, 200, "Aktuelle Schachpartie beenden?", ui_font(UI_TITLE), 540, 4);
        button_h(28, 455, 544, 60, "Neue Partie", 0);
        button_h(28, 540, 544, 60, "Abbrechen", 0);
        break;
    case 27:
        app_calculator();
        break;
    case 28:
        app_timer();
        break;
    case 29:
        app_notes();
        break;
    case 30:
        app_note();
        break;
    case 31:
        app_drawing();
        break;
    case 32:
        app_weather();
        break;
    case 33:
        app_home_assistant();
        break;
    case 34:
        app_ha_setup();
        break;
    case 36:
        app_wiki();
        break;
    case 38:
        app_quotes();
        break;
    case 39:
        app_time_settings();
        break;
    case 40:
        system_screen();
        break;
    case 41:
        app_reset();
        break;
    case 46: {
        heading("Weckzeit");
        button_h(100, 245, 180, 60, "+", 0);
        button_h(320, 245, 180, 60, "+", 0);
        char b[20];
        snprintf(b, sizeof(b), "%02d", alarm_hour);
        centered(100, 320, 180, 80, b, ui_font(UI_DISPLAY));
        snprintf(b, sizeof(b), "%02d", alarm_minute);
        centered(320, 320, 180, 80, b, ui_font(UI_DISPLAY));
        centered(280, 320, 40, 80, ":", ui_font(UI_DISPLAY));
        button_h(100, 415, 180, 60, "−", 0);
        button_h(320, 415, 180, 60, "−", 0);
        button_h(28, 554, 544, 60, "Fertig", 0);
        break;
    }
    case 47: {
        heading("Datum & Uhrzeit");
        const char *names[] = {"Tag", "Monat", "Jahr", "Stunde", "Minute"};
        int values[] = {time_day, time_month, time_year, time_hour, time_minute};
        char b[40];
        for (int i = 0; i < 5; i++) {
            int y = 170 + i * 86;
            drawtext(30, y + 10, names[i], ui_font(UI_BODY), 170);
            snprintf(b, sizeof(b), i == 2 ? "%04d" : "%02d", values[i]);
            centered(200, y, 170, 54, b, ui_font(UI_PRIMARY));
            button_h(390, y, 72, 54, "−", 0);
            button_h(480, y, 92, 54, "+", 0);
        }
        button_h(28, 650, 544, 56, "Fertig", 0);
        break;
    }
    case 42:
        app_calc_history();
        break;
    case 43:
    case 44:
    case 45:
        heading("Löschen?");
        wrapped(30, 180,
                screen == 43   ? "Diese Notiz endgültig löschen?"
                : screen == 45 ? "Diese Tagesnotiz löschen?"
                               : "Dieses Zeichenblatt leeren?",
                ui_font(UI_TITLE), 540, 4);
        button(28, 455, 544, "Löschen", 0);
        button(28, 530, 544, "Abbrechen", 0);
        break;
    }
}
static void extra_return(int target) {
    for (int i = history_count - 1; i >= 0; i--)
        if (history[i] == target) {
            history_count = i;
            break;
        }
    route_to(target);
}
static void extra_commit(int target, const char *text) {
    if (u15_commit(target, text))
        return;
    if (new_apps_commit(target, text))
        return;
    if (target == 61) {
        snprintf(ha_search, sizeof(ha_search), "%s", text);
        ha_page = 0;
        return;
    }
    if (target == 60) {
        scratch_store(text);
        return;
    }
    if (target == 30) {
        if (note_selected < 0) {
            int id = (int)time(NULL);
            while (1) {
                int exists = 0;
                for (int i = 0; i < note_count; i++)
                    if (note_ids[i] == id)
                        exists = 1;
                if (!exists)
                    break;
                id++;
            }
            note_selected = id;
            v1_note_created(id);
        }
        snprintf(note_text, sizeof(note_text), "%s", text);
        char path[80];
        snprintf(path, sizeof(path), "notes/%d.txt", note_selected);
        extra_write_text(path, note_text);
        notes_scan();
        if (screen == 29 && history_count < 64 &&
            (!history_count || history[history_count - 1] != 29))
            history[history_count++] = 29;
        extra_return(30);
        extra_page = 0;
    } else if (target == 31) {
        if (!service_url_valid(text)) {
            ui_message("Ungültige Serveradresse.");
            return;
        }
        snprintf(ha_url, sizeof(ha_url), "%s", text);
        extra_write_text("ha-url.txt", ha_url);
    } else if (target == 32) {
        if (strpbrk(text, "\r\n\"\\")) {
            ui_message("Ungültiger Token.");
            return;
        }
        snprintf(ha_token, sizeof(ha_token), "%s", text);
        extra_write_text("ha-token.txt", ha_token);
    } else if (target == 33) {
        if (!*text)
            return;
        snprintf(weather_city, sizeof(weather_city), "%s", text);
        extra_write_text("weather-city.txt", weather_city);
        if (!service_start(SERVICE_WEATHER, weather_city, "", ""))
            ui_message("Anfrage läuft bereits.");
    } else if (target == 34) {
        wiki_article_language = options[OPT_LANGUAGE];
        snprintf(wiki_query, sizeof(wiki_query), "%s", text);
        wiki_page = 0;
        extra_write_text("wiki-query.txt", wiki_query);
        discover_lookup(wiki_query, options[OPT_LANGUAGE]);
    } else if (target == 35) {
        int y, m, d, h, minute;
        char tail;
        if (sscanf(text, "%d-%d-%d %d:%d%c", &y, &m, &d, &h, &minute, &tail) != 5 ||
            !system_set_time(y, m, d, h, minute))
            ui_message("Ungültiges Datum / Uhrzeit.");
        else {
            status_time = 0;
            ui_message("Datum und Uhrzeit gesetzt.");
        }
    } else if (target == 36) {
        char *end;
        long n = strtol(text, &end, 10);
        if (!*end && n >= 1 && n <= 1440)
            timer_minutes = (int)n;
        else
            ui_message("1 bis 1440 Minuten eingeben.");
        extra_save();
    } else if (target == 37) {
        int h, m;
        char c;
        if (sscanf(text, "%d:%d%c", &h, &m, &c) == 2 && h >= 0 && h < 24 && m >= 0 && m < 60) {
            alarm_hour = h;
            alarm_minute = m;
        } else
            ui_message("Uhrzeit als HH:MM eingeben.");
        extra_save();
    } else if (target == 40) {
        int y, m;
        char tail;
        if (sscanf(text, "%d-%d%c", &y, &m, &tail) == 2 && y >= 1900 && y <= 2100 && m >= 1 &&
            m <= 12) {
            calendar_year = y;
            calendar_month = m;
            if (calendar_day > calendar_days(y, m))
                calendar_day = calendar_days(y, m);
            app_calendar_load();
        } else
            ui_message("Ungültiger Monat.");
    } else if (target == 38) {
        int v = atoi(text);
        if (v >= 1 && v <= 240)
            reading_goal = v;
        extra_save();
    }
}
static int64_t drawing_next_present;
static int extra_input(const Input *in) {
    int x = in->x, y = in->y;
    if (screen == 48 || screen == 49 || screen == 50)
        return games_input(in);
    if (screen == 31 &&
        (in->kind == INPUT_DOWN || in->kind == INPUT_MOVE || in->kind == INPUT_TAP ||
         in->kind == INPUT_SWIPE) &&
        ((in->kind == INPUT_DOWN && y >= 112 && y < 800) ||
         (in->kind != INPUT_DOWN && down_y >= 112 && down_y < 800))) {
        if (in->kind == INPUT_DOWN) {
            memcpy(drawing_undo, drawing, sizeof(drawing));
            draw_x = draw_y = -1;
        }
        draw_line(x, y);
        if (in->kind == INPUT_TAP || in->kind == INPUT_SWIPE) {
            down_y = -1;
            draw_x = draw_y = -1;
            drawing_save();
        }
        if (in->kind == INPUT_MOVE && extra_millis() < drawing_next_present)
            return 2;
        drawing_next_present = extra_millis() + 250;
        return 1;
    }
    if (in->kind != INPUT_TAP)
        return 2;
    if (screen == 22) {
        if (y >= 128 && y < 166) {
            stats_page = (x - 28) / 186;
            if (stats_page < 0)
                stats_page = 0;
            if (stats_page > 2)
                stats_page = 2;
            extra_page = 0;
        } else if (stats_page == 0 && y >= 484 && y < 528 && x >= 390) {
            char b[32];
            snprintf(b, sizeof(b), "%d", reading_goal);
            open_keyboard(38, "Tagesziel: Minuten", b);
        } else if (stats_page == 1 && y >= 679 && y < 723) {
            if (x < 300 && extra_page)
                extra_page--;
            else if (x >= 300) {
                int n = 0;
                for (int i = 0; i < library_count; i++) {
                    BookInsight *v = insight_get(library[i].id, 0);
                    if (v && (v->seconds || v->pages))
                        n++;
                }
                if ((extra_page + 1) * 5 < n)
                    extra_page++;
            }
        }
        return 1;
    }

    if (screen == 27) {
        if (y >= 74 && y < 118 && x >= 408 && x < 460) {
            if (!*calc_expr)
                snprintf(calc_expr, sizeof(calc_expr), "sin(x)");
            extra_open(61);
            return 1;
        }
        if (y >= 74 && y < 118 && x >= 460 && x < 516) {
            extra_open(42);
            return 1;
        }
        if (y >= 361 && y < 413 && x >= 528) {
            calc_backspace();
            return 1;
        }
        if (y >= 361 && y < 413) {
            int start = calc_input_start();
            int w = 43;
            const char *p = calc_expr + text_boundary(calc_expr, start);
            while (*p) {
                const char *next = p;
                unsigned c = glyph(&next);
                int a = glyph_advance(ui_font(UI_BODY), c);
                if (x < w + a / 2)
                    break;
                w += a;
                p = next;
            }
            calc_cursor = p - calc_expr;
            return 1;
        }
        if (y >= 429 && y < 531 && x >= 28 && x < 572) {
            if ((!calc_alt && x >= 534) || (calc_alt && x < 66)) {
                calc_alt ^= 1;
                return 1;
            }
            int r = (y - 429) / 51, c = (x - calc_function_left()) / calc_function_pitch();
            if (c >= 0 && c < 8 && y < 429 + r * 51 + 44 &&
                x < calc_function_left() + c * calc_function_pitch() + 57) {
                const char *key = (calc_alt ? calc_all1 : calc_all0)[r * 8 + c];
                if (!strcmp(key, "M+")) {
                    double value;
                    char err[80];
                    calc_memory += calc_evaluate(calc_expr, calc_degrees, calc_answer, calc_memory,
                                                 &value, err, sizeof(err))
                                       ? value
                                       : calc_answer;
                } else if (!strcmp(key, "MC"))
                    calc_memory = 0;
                else if (isalpha((unsigned char)key[0]) && strcmp(key, "pi") && strcmp(key, "e") &&
                         strcmp(key, "ans") && strcmp(key, "mem") && strcmp(key, "x") &&
                         strcmp(key, "y")) {
                    calc_insert(key);
                    calc_insert("(");
                } else
                    calc_insert(key);
            }
            return 1;
        }
        if (y >= 540 && y < 782) {
            if (x >= 468) {
                if (y >= 664)
                    calc_run();
                else if (y < 596) {
                    if (!*calc_expr)
                        calc_insert("-");
                    else if (strlen(calc_expr) < sizeof(calc_expr) - 4) {
                        char opposite[1024];
                        snprintf(opposite, sizeof(opposite), "-(%s)", calc_expr);
                        snprintf(calc_expr, sizeof(calc_expr), "%s", opposite);
                        calc_cursor = strlen(calc_expr);
                        calc_preview();
                    }
                } else if (y >= 602 && y < 658) {
                    calc_expr[0] = calc_result[0] = calc_error[0] = 0;
                    calc_cursor = 0;
                }
            } else if (x >= 28 && x < 452) {
                int c = (x - 28) / 108, r = (y - 540) / 62;
                if (c < 4 && r < 4 && x < 28 + c * 108 + 100 && y < 540 + r * 62 + 56)
                    calc_insert(calc_digits[r * 4 + c]);
            }
            return 1;
        }
        return 1;
    }
    if (screen == 42) {
        if (y >= 145 && y < 657) {
            int i = calc_history_count - 1 - (extra_page * 8 + (y - 145) / 64);
            if (i >= 0) {
                snprintf(calc_expr, sizeof(calc_expr), "%s", calc_history[i]);
                calc_cursor = strlen(calc_expr);
                extra_return(27);
                calc_preview();
                calc_error[0] = 0;
            }
        } else if (y >= 679 && y < 723) {
            if (x < 300 && extra_page)
                extra_page--;
            else if (x >= 300 && (extra_page + 1) * 8 < calc_history_count)
                extra_page++;
        }
        return 1;
    }
    if (screen == 28) {
        if (y >= 185 && y < 229 && x >= 516) {
            extra_open(87);
            return 1;
        }
        if (y >= 133 && y < 177) {
            timer_panel = x >= 300;
            return 1;
        }
        if (!timer_panel) {
            if (y >= 397 && y < 445 && x >= 28) {
                const int presets[] = {5, 10, 20, 30};
                int i = (x - 28) / 140;
                if (i < 4)
                    timer_minutes = presets[i];
            } else if (y >= 472 && y < 524) {
                if (x < 108) {
                    if (timer_minutes > 1)
                        timer_minutes--;
                } else if (x >= 492) {
                    if (timer_minutes < 1440)
                        timer_minutes++;
                } else {
                    char b[32];
                    snprintf(b, sizeof(b), "%d", timer_minutes);
                    open_keyboard(36, "Timer: Minuten", b);
                }
            } else if (y >= 566 && y < 630) {
                timer_action(x >= 300);
            }
        } else if (y >= 292 && y < 386) {
            extra_open(46);
        } else if (y >= 441 && y < 501 && x >= 460)
            alarm_enabled ^= 1;
        extra_save();
        return 1;
    }
    if (screen == 46) {
        if (y >= 245 && y < 305) {
            if (x < 300)
                alarm_hour = (alarm_hour + 1) % 24;
            else
                alarm_minute = (alarm_minute + 1) % 60;
        } else if (y >= 415 && y < 475) {
            if (x < 300)
                alarm_hour = (alarm_hour + 23) % 24;
            else
                alarm_minute = (alarm_minute + 59) % 60;
        } else if (y >= 554 && y < 614) {
            extra_save();
            extra_return(28);
        }
        return 1;
    }
    if (screen == 29)
        return v1_notes_input(in);
    if (screen == 30) {
        if (y >= 54 && y < 98 && x >= 524) {
            extra_open(85);
            return 1;
        }
        if (y >= 54 && y < 740) {
            open_keyboard(30, "Notiz", note_text);
            note_cursor = strlen(edit);
            note_view_start = extra_page * 23;
            note_editor_touch(x, y);
        } else if (y >= 748) {
            if (x < 300 && extra_page)
                extra_page--;
            else if (x >= 300) {
                const char *p = note_text;
                for (int i = 0; i < (extra_page + 1) * 23 && *p; i++)
                    p = note_next_line(p, 536);
                if (*p)
                    extra_page++;
            }
        }
        return 1;
    }
    if (screen == 31 && y >= 57 && y < 105) {
        if (x < 100) {
            drawing_save();
            drawing_slot = (drawing_slot + 1) % drawing_count;
            drawing_load();
        } else if (x < 195) {
            drawing_save();
            if (x < 148 && drawing_slot > 0)
                drawing_slot--;
            else if (x >= 148 && drawing_slot + 1 < drawing_count)
                drawing_slot++;
            drawing_load();
        } else if (x < 250) {
            drawing_save();
            drawing_slot = drawing_count++;
            memset(drawing, 255, sizeof(drawing));
            memcpy(drawing_undo, drawing, sizeof(drawing));
            drawing_ready = drawing_dirty = 1;
            drawing_save();
        } else if (x < 375)
            drawing_eraser ^= 1;
        else if (x < 445)
            v1_export_drawing();
        else if (x < 510) {
            for (size_t i = 0; i < sizeof(drawing); i++) {
                unsigned char c = drawing[i];
                drawing[i] = drawing_undo[i];
                drawing_undo[i] = c;
            }
            drawing_dirty = 1;
        } else
            extra_open(44);
        return 1;
    }
    if (screen == 32) {
        if (y >= 74 && y < 118) {
            if (x >= 516) {
                if (!service_start(SERVICE_WEATHER, weather_city, "", ""))
                    ui_message("Anfrage läuft bereits.");
            } else if (x >= 460)
                open_keyboard(33, "Wetter: Ort", weather_city);
        }
        return 1;
    }
    if (screen == 33) {
        if (y >= 74 && y < 118) {
            if (x >= 516) {
                ha_select ^= 1;
                ha_page = 0;
            } else if (x >= 456)
                extra_open(34);
        } else if ((!*ha_url || !*ha_token) && y >= 420 && y < 468)
            extra_open(34);
        else if (ha_select && y >= 187 && y < 235) {
            open_keyboard(61, "Geräte suchen", ha_search);
        } else if ((ha_select ? y >= 250 && y < 658
                              : y >= 198 && y < 660 && ((y - 198) % 154 < 140) &&
                                    (x < 290 || x >= 310)) &&
                   x >= 28 && x < 572) {
            int v[512], n = ha_visible(v);
            int cell = ha_select ? (y - 250) / 68 : ((y - 198) / 154) * 2 + (x >= 310);
            int row = ha_page * 6 + cell;
            if (row < n) {
                HAEntity *e = &ha_entities[v[row]];
                if (ha_select) {
                    int fav = ha_favorite(e->id);
                    if (fav >= 0) {
                        memmove(ha_favorites + fav, ha_favorites + fav + 1,
                                (ha_favorite_count - fav - 1) * sizeof(ha_favorites[0]));
                        ha_favorite_count--;
                    } else if (ha_favorite_count < 128)
                        snprintf(ha_favorites[ha_favorite_count++], 160, "%s", e->id);
                    char all[22000] = "";
                    for (int i = 0; i < ha_favorite_count; i++) {
                        strcat(all, ha_favorites[i]);
                        strcat(all, "\n");
                    }
                    extra_write_text("ha-favorites.txt", all);
                } else if (e->sensor) {
                    snprintf(ha_plot_id, sizeof(ha_plot_id), "%s", e->id);
                    extra_open(60);
                    return 1;
                } else if (!strcmp(e->state, "unavailable") || !strcmp(e->state, "unknown"))
                    ui_message("Gerät nicht verfügbar.");
                else if (!service_start(SERVICE_TOGGLE, ha_url, ha_token, e->id))
                    ui_message("Anfrage läuft bereits.");
            }
        } else if (y >= 679 && y < 723) {
            if (x < 300 && ha_page)
                ha_page--;
            else if (x >= 300) {
                int view[512], n = ha_visible(view);
                if ((ha_page + 1) * 6 < n)
                    ha_page++;
            }
        }
        return 1;
    }
    if (screen == 34) {
        if (y >= 188 && y < 240)
            open_keyboard(31, "Home Assistant: URL", ha_url);
        else if (y >= 327 && y < 379)
            open_keyboard(32, "Zugriffstoken", ha_token);
        else if (y >= 607 && y < 655) {
            if (service_start(SERVICE_HA, ha_url, ha_token, "")) {
                ha_select = 1;
                ha_page = 0;
                extra_return(33);
            } else
                ui_message("Adresse und Token prüfen.");
        }
        return 1;
    }
    if (screen == 51) {
        return 1;
    }
    if (screen == 36)
        return wiki_history_input(in);
    if (screen == 38) {
        if (y >= 146 && y < 650) {
            int row = -1;
            for (int r = 0; r < quote_hit_count; r++)
                if (y >= quote_hit_top[r] && y < quote_hit_bottom[r]) {
                    row = r;
                    break;
                }
            int i = global_quote_page * 4 + row;
            if (row >= 0 && i < global_quote_count) {
                GlobalQuote q;
                if (quote_get(i, &q)) {
                    AppIntent request = {.app = APP_READER,
                                         .kind = INTENT_BOOK,
                                         .object_id = library[q.book].id,
                                         .has_position = 1,
                                         .position = q.position};
                    app_send(&request);
                }
            }
        } else if (y >= 679 && y < 723) {
            if (x < 300 && global_quote_page)
                global_quote_page--;
            else if (x >= 300 && (global_quote_page + 1) * 4 < global_quote_count)
                global_quote_page++;
        }
        return 1;
    }
    if (screen == 39) {
        if (y >= 286 && y < 348) {
            options[OPT_AUTO_TIME] ^= 1;
            library_settings_save();
            if (options[OPT_AUTO_TIME] && !service_busy())
                service_start(SERVICE_TIME, "", "", "");

        } else if (!options[OPT_AUTO_TIME] && y >= 387 && y < 441) {
            extra_open(47);
        } else if (!options[OPT_AUTO_TIME] && y >= 480 && y < 532)
            open_dropdown(100 + OPT_ZONE, zone_names, 5, options[OPT_ZONE], 220, 540, 350);
        return 1;
    }
    if (screen == 47) {
        if (y >= 170 && y < 568) {
            int row = (y - 170) / 86, delta = x >= 480 ? 1 : x >= 390 ? -1 : 0;
            if (row == 0) {
                time_day += delta;
                int max = calendar_days(time_year, time_month);
                if (time_day < 1)
                    time_day = max;
                if (time_day > max)
                    time_day = 1;
            }
            if (row == 1) {
                time_month += delta;
                if (time_month < 1)
                    time_month = 12;
                if (time_month > 12)
                    time_month = 1;
            }
            if (row == 2) {
                time_year += delta;
                if (time_year < 2000)
                    time_year = 2000;
                if (time_year > 2100)
                    time_year = 2100;
            }
            if (row == 3)
                time_hour = (time_hour + delta + 24) % 24;
            if (row == 4)
                time_minute = (time_minute + delta + 60) % 60;
            int max = calendar_days(time_year, time_month);
            if (time_day > max)
                time_day = max;
        } else if (y >= 650 && y < 706) {
            if (system_set_time(time_year, time_month, time_day, time_hour, time_minute)) {
                status_time = 0;
                extra_return(39);
            } else
                ui_message("Ungültiges Datum / Uhrzeit.");
        }
        return 1;
    }
    if (screen == 41) {
        if (y >= 455 && y < 503) {
            library_defaults();
            extra_zone();
            apply_reading_settings();
            char p[2048];
            local_path(p, sizeof(p), "../DISABLE_AUTOSTART");
            unlink(p);
            route_to(13);
            history_count = 0;
            ui_message("Standardeinstellungen wiederhergestellt.");
        } else if (y >= 530 && y < 578)
            extra_return(40);
        return 1;
    }
    if (screen == 45) {
        if (y >= 455 && y < 503) {
            apps_note_commit("");
            extra_return(21);
        } else if (y >= 530 && y < 578)
            extra_return(21);
        return 1;
    }
    if (screen == 43 || screen == 44) {
        if (y >= 455 && y < 503) {
            if (screen == 43) {
                char p[2048], name[80];
                snprintf(name, sizeof(name), "notes/%d.txt", note_selected);
                local_path(p, sizeof(p), name);
                if (access(p, F_OK))
                    ui_message("Löschen fehlgeschlagen.");
                else {
                    note_delete16(note_selected);
                    notes_scan();
                    extra_return(29);
                }
            } else {
                memcpy(drawing_undo, drawing, sizeof(drawing));
                memset(drawing, 255, sizeof(drawing));
                drawing_dirty = 1;
                extra_return(31);
                drawing_save();
            }
        } else if (y >= 530 && y < 578)
            extra_return(screen == 43 ? 29 : 31);
        return 1;
    }
    return 1;
}
static int reading_streak(void) {
    time_t now = time(NULL);
    struct tm t;
    localtime_r(&now, &t);
    t.tm_hour = 12;
    t.tm_min = t.tm_sec = 0;
    int streak = 0;
    for (int i = 0; i < 31; i++) {
        time_t d = mktime(&t);
        ReadingDay *day = app_day(app_date_key(d), 0);
        if (day && day->seconds)
            streak++;
        else if (i > 0)
            break;
        t.tm_mday--;
    }
    return streak;
}
static void app_insights(void) {
    heading("Lesestatistik");
    const char *tabs[] = {"Überblick", "Bücher", "30 Tage"};
    segments(28, 128, 544, 42, tabs, 3, stats_page);
    if (stats_page == 0) {
        ReadingDay *today = app_day(app_date_key(time(NULL)), 0);
        unsigned mins = today ? today->seconds / 60 : 0;
        char b[100];
        snprintf(b, sizeof(b), "%u / %d min", mins, reading_goal);
        stat_card(28, 196, "Heute", b);
        snprintf(b, sizeof(b), "%d %s", reading_streak(), tr("Tage"));
        stat_card(312, 196, "Leseserie", b);
        snprintf(b, sizeof(b), "%u h %02u m", app_data.seconds / 3600, app_data.seconds / 60 % 60);
        stat_card(28, 326, "Lesezeit", b);
        snprintf(b, sizeof(b), "%u", insights.sessions);
        stat_card(312, 326, "Lesesitzungen", b);
        progress(28, 456, 544, (int)mins, reading_goal);
        drawtext(30, 495, "Mein Tagesziel", ui_font(UI_BODY), 330);
        button_h(390, 484, 182, 44, "Ändern", 0);
        if (current >= 0) {
            BookInsight *r = insight_get(library[current].id, 0);
            int remaining = engine_count() - page - 1;
            snprintf(b, sizeof(b), "%s: %d %s", tr("Aktuelles Buch"), remaining,
                     tr("Seiten übrig"));
            drawtext(30, 562, b, ui_font(UI_BODY), 540);
            if (r && r->pages >= 3) {
                snprintf(b, sizeof(b), "~ %u min %s",
                         (unsigned)((double)r->seconds / r->pages * remaining / 60),
                         tr("bis zum Ende"));
                drawtext(30, 607, b, ui_font(UI_BODY), 540);
            }
        }
        time_t now = time(NULL);
        struct tm t;
        localtime_r(&now, &t);
        t.tm_hour = 12;
        unsigned week = 0, goals = 0;
        for (int d = 0; d < 7; d++) {
            ReadingDay *day = app_day(app_date_key(mktime(&t)), 0);
            if (day) {
                week += day->seconds / 60;
                goals += day->seconds >= (unsigned)reading_goal * 60;
            }
            t.tm_mday--;
        }
        char motivation[160];
        snprintf(motivation, sizeof(motivation), "%u min %s · %u / 7 %s", week, tr("diese Woche"),
                 goals, tr("Tagesziele"));
        drawtext(30, 650, motivation, ui_font(UI_BODY), 540);
        snprintf(motivation, sizeof(motivation),
                 mins >= (unsigned)reading_goal ? "%s" : "%s %u min",
                 tr(mins >= (unsigned)reading_goal ? "Tagesziel erreicht. Schön gelesen!"
                                                   : "Bis zum Tagesziel:"),
                 mins >= (unsigned)reading_goal ? 0 : (unsigned)reading_goal - mins);
        wrapped(30, 691, motivation, ui_font(UI_SECONDARY), 540, 2);
    } else if (stats_page == 1) {
        int rows[BOOK_LIMIT], n = 0;
        BookInsight *row_values[BOOK_LIMIT];
        for (int b = 0; b < library_count && n < BOOK_LIMIT; b++) {
            BookInsight *v = insight_get(library[b].id, 0);
            if (v && (v->seconds || v->pages)) {
                rows[n] = b;
                row_values[n++] = v;
            }
        }
        for (int i = 0; i < n; i++)
            for (int k = i + 1; k < n; k++)
                if (row_values[k]->seconds > row_values[i]->seconds) {
                    int z = rows[i];
                    rows[i] = rows[k];
                    rows[k] = z;
                    BookInsight *value = row_values[i];
                    row_values[i] = row_values[k];
                    row_values[k] = value;
                }
        for (int r = 0; r < 5 && extra_page * 5 + r < n; r++) {
            Book *b = &library[rows[extra_page * 5 + r]];
            BookInsight *v = insight_get(b->id, 0);
            int y = 190 + r * 91;
            drawtext_raw(30, y, b->title, ui_font(UI_BODY), 540);
            char line[120];
            snprintf(line, sizeof(line), "%u min  ·  %u %s  ·  %u %s", v->seconds / 60, v->pages,
                     tr("Seiten"), v->sessions, tr("Sitzungen"));
            drawtext(30, y + 35, line, ui_font(UI_SECONDARY), 540);
            progress(30, y + 66, 540, b->page + 1, b->total);
        }
        if (!n)
            wrapped(30, 220, "Deine Leseaktivität wird ab jetzt pro Buch erfasst.", ui_font(UI_TITLE), 540,
                    4);
        pager(679, &extra_page, (n + 4) / 5);
    } else {
        unsigned secs[30], sum = 0, max = 60, active = 0;
        time_t now = time(NULL);
        struct tm t;
        localtime_r(&now, &t);
        t.tm_hour = 12;
        t.tm_min = t.tm_sec = 0;
        t.tm_mday -= 29;
        for (int i = 0; i < 30; i++) {
            time_t d = mktime(&t);
            ReadingDay *r = app_day(app_date_key(d), 0);
            secs[i] = r ? r->seconds : 0;
            sum += secs[i];
            if (secs[i])
                active++;
            if (secs[i] > max)
                max = secs[i];
            t.tm_mday++;
        }
        char b[80];
        snprintf(b, sizeof(b), "%u %s  ·  %.1f h", active, tr("aktive Tage"), sum / 3600.0);
        drawtext(30, 214, b, ui_font(UI_PRIMARY), 540);
        for (int i = 0; i < 30; i++) {
            int h = (int)(240ULL * secs[i] / max);
            soft_rect(30 + i * 18, 565 - h, 12, h ? h : 2, 2, secs[i] ? 0 : 220);
        }
        drawtext(30, 590, "Vor 30 Tagen", ui_font(UI_SECONDARY), 260);
        drawtext(490, 590, "Heute", ui_font(UI_SECONDARY), 82);
        snprintf(b, sizeof(b), "Ø %u min / %s", active ? sum / 60 / active : 0, tr("aktivem Tag"));
        drawtext(30, 653, b, ui_font(UI_BODY), 540);
    }
}

static void extra_wiki_restore(void) {
    char path[2048];
    local_path(path, sizeof(path), "wiki-last.xml");
    if (!access(path, R_OK))
        discover_parse_lookup(path);
}
