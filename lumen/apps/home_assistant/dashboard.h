/* Read-only sensor history; actuator actions remain in the existing input path. */
#define HA_POINTS 128
typedef struct {
    char id[160];
    double value[HA_POINTS];
    time_t time[HA_POINTS];
    int count;
} HAHistory;
static HAHistory ha_history[8];
static char ha_history_key[1500];
static time_t ha_history_at, ha_refresh_at;
static char ha_search[128], ha_plot_id[160];
static const char *ha_state_label(const char *state) {
    if (!strcmp(state, "on")) return tr("Ein");
    if (!strcmp(state, "off")) return tr("Aus");
    if (!strcmp(state, "unavailable")) return tr("Nicht verfügbar");
    if (!strcmp(state, "unknown")) return tr("Unbekannt");
    return state;
}
static int ha_back(void) {
    if (ui_route() != 33 || !ha_select) return 0;
    ha_select = ha_page = 0;
    ha_search[0] = 0;
    return 1;
}

static time_t ha_timestamp(const char *s) {
    struct tm t = {0};
    int y, m, d, h, min;
    double sec;
    if (sscanf(s, "%d-%d-%dT%d:%d:%lf", &y, &m, &d, &h, &min, &sec) != 6)
        return 0;
    t.tm_year = y - 1900;
    t.tm_mon = m - 1;
    t.tm_mday = d;
    t.tm_hour = h;
    t.tm_min = min;
    t.tm_sec = (int)sec;
    time_t v = timegm(&t);
    const char *p = s + 19;
    if (*p == '.') {
        p++;
        while (isdigit((unsigned char)*p))
            p++;
    }
    if (*p == '+' || *p == '-') {
        int oh = 0, om = 0;
        if (sscanf(p + 1, "%d:%d", &oh, &om) == 2)
            v += (*p == '+' ? -1 : 1) * (oh * 3600 + om * 60);
    }
    return v;
}
static void ha_history_parse(void) {
    char *s = service_read("ha-history.json", 2097152);
    if (!s)
        return;
    JsonToken *tok = calloc(128000, sizeof(*tok));
    Json j;
    if (tok && json_parse(&j, s, tok, 128000) && j.tokens[0].type == '[') {
        memset(ha_history, 0, sizeof(ha_history));
        for (int a = 0; a < j.tokens[0].count && a < 8; a++) {
            int array = json_at(&j, 0, a), first = json_at(&j, array, 0);
            if (first < 0)
                continue;
            HAHistory *hist = &ha_history[a];
            json_string(&j, json_get(&j, first, "entity_id"), hist->id, sizeof(hist->id));
            int count = j.tokens[array].count,
                step = count > HA_POINTS ? (count + HA_POINTS - 2) / (HA_POINTS - 1) : 1;
            for (int k = 0; k < count; k++) {
                if (k % step && k != count - 1)
                    continue;
                if (hist->count >= HA_POINTS)
                    break;
                int obj = json_at(&j, array, k);
                char state[80], stamp[80];
                json_string(&j, json_get(&j, obj, "state"), state, sizeof(state));
                json_string(&j, json_get(&j, obj, "last_changed"), stamp, sizeof(stamp));
                char *end;
                double v = strtod(state, &end);
                if (!strcmp(state, "on"))
                    v = 1;
                else if (!strcmp(state, "off"))
                    v = 0;
                else if (end == state || *end || !isfinite(v))
                    continue;
                time_t t = ha_timestamp(stamp);
                if (!t)
                    continue;
                int at = hist->count++;
                hist->value[at] = v;
                hist->time[at] = t;
            }
        }
    }
    free(tok);
    free(s);
}
static int ha_visible(int *out) {
    int n = 0;
    char query[sizeof(ha_search)];
    utf8_fold(ha_search, query, sizeof(query));
    for (int i = 0; i < ha_count; i++) {
        const HAEntity *e = &ha_entities[i];
        if (!ha_select && ha_favorite(e->id) < 0) continue;
        if (ha_select && *query) {
            char name[sizeof(e->name)], id[sizeof(e->id)];
            utf8_fold(e->name, name, sizeof(name));
            utf8_fold(e->id, id, sizeof(id));
            if (!strstr(name, query) && !strstr(id, query)) continue;
        }
        out[n++] = i;
    }
    return n;
}
static int ha_request_history(void) {
    if ((ui_route() != 33 && ui_route() != 60) || ha_select || service_history_busy() || !*ha_url ||
        !*ha_token)
        return 0;
    int view[512], n = ha_visible(view);
    char ids[1500] = "";
    for (int r = 0; r < (ha_select ? 6 : 8) && ha_page * (ha_select ? 6 : 8) + r < n; r++) {
        HAEntity *e = &ha_entities[view[ha_page * (ha_select ? 6 : 8) + r]];
        if (!e->sensor)
            continue;
        if (*ids)
            strcat(ids, ",");
        strncat(ids, e->id, sizeof(ids) - strlen(ids) - 1);
    }
    if (ui_route() == 60)
        snprintf(ids, sizeof(ids), "%s", ha_plot_id);
    if (!*ids)
        return 0;
    if (!strcmp(ids, ha_history_key) && time(NULL) - ha_history_at < 300)
        return 0;
    if (service_start(SERVICE_HA_HISTORY, ha_url, ha_token, ids)) {
        snprintf(ha_history_key, sizeof(ha_history_key), "%s", ids);
        ha_history_at = time(NULL);
        return 1;
    }
    return 0;
}
static void ha_graph(HAEntity *e, int x, int y, int w, int h) {
    HAHistory *hist = NULL;
    for (int i = 0; i < 8; i++)
        if (!strcmp(e->id, ha_history[i].id))
            hist = &ha_history[i];
    if (!hist || !hist->count) {
        ink_color = UI_SECONDARY_INK;
        drawtext(x, y + h - 22, service_history_busy() ? "Verlauf wird geladen …" : "Kein Verlauf",
                 ui_font(UI_SECONDARY), w);
        ink_color = 0;
        return;
    }
    double lo = hist->value[0], hi = lo;
    for (int i = 1; i < hist->count; i++) {
        if (hist->value[i] < lo)
            lo = hist->value[i];
        if (hist->value[i] > hi)
            hi = hist->value[i];
    }
    if (hi - lo < .000001) {
        hi += .5;
        lo -= .5;
    }
    time_t end = time(NULL), start = end - 86400;
    int px = x, py = y + h / 2, seen = 0;
    for (int i = 0; i < hist->count; i++) {
        if (hist->time[i] < start && i + 1 < hist->count && hist->time[i + 1] < start)
            continue;
        double f = (double)(hist->time[i] - start) / 86400;
        if (f < 0)
            f = 0;
        if (f > 1)
            f = 1;
        int nx = x + (int)(f * (w - 1)),
            ny = y + h - 4 - (int)((hist->value[i] - lo) / (hi - lo) * (h - 8));
        if (seen) {
            for (int xx = px; xx <= nx; xx++) {
                int yy = nx == px ? ny : py + (ny - py) * (xx - px) / (nx - px);
                rect(xx, yy, 1, y + h - yy, 235);
            }
        }
        px = nx;
        py = ny;
        seen = 1;
    }
    // Redraw the curve after its area fill, without changing text ink.
    seen = 0;
    for (int i = 0; i < hist->count; i++) {
        double f = (double)(hist->time[i] - start) / 86400;
        if (f < 0)
            f = 0;
        if (f > 1)
            f = 1;
        int nx = x + (int)(f * (w - 1)),
            ny = y + h - 4 - (int)((hist->value[i] - lo) / (hi - lo) * (h - 8));
        if (seen) {
            for (int xx = px; xx <= nx; xx++) {
                int yy = nx == px ? ny : py + (ny - py) * (xx - px) / (nx - px);
                rect(xx, yy, 1, 3, 55);
            }
        }
        px = nx;
        py = ny;
        seen = 1;
    }
    if (seen) {
        rect(px, py, 1, y + h - py, 235);
        rect(px, py, 2, 2, 110);
    }
    ink_color = 105;
    drawtext_raw(x + w - 32, y + h - 18, "24 h", ui_font(UI_SECONDARY), 32);
    ink_color = 0;
}
static void ha_dashboard(void) {
    heading_actions("Home Assistant", 2);
    icon_button(468, 74, IC_SETTINGS);
    icon_button(528, 74, ha_select ? IC_DONE : IC_DEVICES);
    if (!*ha_url || !*ha_token) {
        wrapped(30, 170,
                "Serveradresse und Zugriffstoken einrichten. Danach Schalter für dein Dashboard "
                "auswählen.",
                ui_font(UI_TITLE), 540, 5);
        button(30, 420, 540, "Einrichten", 0);
        return;
    }
    int view[512], n = ha_visible(view);
    if (ha_page * (ha_select ? 6 : 8) >= n)
        ha_page = n ? (n - 1) / (ha_select ? 6 : 8) : 0;
    if (ha_select)
        drawtext(30, 140, "Geräte auswählen", ui_font(UI_SECONDARY), 540);
    if (ha_select)
        search_field(28, 187, 544, ha_search, "Geräte suchen");
    for (int r = 0; r < (ha_select ? 6 : 8) && ha_page * (ha_select ? 6 : 8) + r < n; r++) {
        HAEntity *e = &ha_entities[view[ha_page * (ha_select ? 6 : 8) + r]];
        if (ha_select) {
            int y = 250 + r * 68;
            char b[120];
            snprintf(b, sizeof(b), "%s%s%s", ha_state_label(e->state), *e->unit ? " " : "", e->unit);
            list_row(28, y, 544, 68, -1, 90, e->name, b, ui_font(UI_BODY), 1);
            toggle(492, y + 18, ha_favorite(e->id) >= 0);
            continue;
        }
        int x = 28 + (r % 2) * 282, y = 148 + (r / 2) * 140;
        region(x, y, 262, 126);
        if (e->sensor) {
            rounded_clip_begin(x, y, 262, 126);
            ha_graph(e, x + 3, y + 3, 256, 120);
            rounded_clip_end(x, y, 262, 126);
        }
        widget_shape(x, y, 262, 126, RADIUS_LARGE, 3, 0);
        outline_surface(x, y, 262, 126, RADIUS_LARGE, 48);
        label_ellipsis(x + 13, y + 8, 236, 28, e->name, ui_font(UI_BODY), 0);
        if (e->sensor) {
            char v[100], value[72];
            char *end;
            double number = strtod(e->state, &end);
            if (end != e->state && !*end && isfinite(number))
                snprintf(value, sizeof(value), "%.6g", number);
            else
                snprintf(value, sizeof(value), "%s",
                         ha_state_label(e->state));
            snprintf(v, sizeof(v), "%s%s%s", value, *e->unit ? " " : "", e->unit);
            const Font *f = textwidth_raw(v, ui_font(UI_TITLE)) > 234 ? ui_font(UI_PRIMARY) : ui_font(UI_TITLE);
            label_ellipsis(x + 13, y + 38, 236, 44, v, f, 0);
        } else {
            drawtext(x + 13, y + 52,
                     ha_state_label(e->state),
                     ui_font(UI_BODY), 150);
            toggle(x + 176, y + 55, !strcmp(e->state, "on"));
        }
    }
    if (!n)
        wrapped(30, ha_select ? 270 : 225,
                service_busy() ? "Geräte werden geladen …" :
                ha_select ? "Keine passenden Geräte." : "Keine Geräte ausgewählt.",
                ui_font(UI_TITLE), 540, 4);
    pager(745, &ha_page, (n + (ha_select ? 5 : 7)) / (ha_select ? 6 : 8));
}
