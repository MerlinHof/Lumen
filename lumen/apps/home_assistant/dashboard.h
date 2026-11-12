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
static int ha_visible(int *out,int picking,const char *search) {
    int n = 0;
    char query[128];
    utf8_fold(search, query, sizeof(query));
    for (int i = 0; i < ha_count; i++) {
        const HAEntity *e = &ha_entities[i];
        if (!picking && ha_favorite(e->id) < 0) continue;
        if (picking && *query) {
            char name[sizeof(e->name)], id[sizeof(e->id)];
            utf8_fold(e->name, name, sizeof(name));
            utf8_fold(e->id, id, sizeof(id));
            if (!strstr(name, query) && !strstr(id, query)) continue;
        }
        out[n++] = i;
    }
    return n;
}
