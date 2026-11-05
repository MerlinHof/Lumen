/* App-owned module; assembled by apps/host/compose.h. */
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
        /* Single outline union, no seams between cloud lobes. */
        for (int yy = y; yy <= y + size; yy++)
            for (int xx = x; xx <= x + size; xx++) {
                float a = hypotf(xx - (x + size * .34f), yy - (y + size * .54f)) - size * .15f;
                float b = hypotf(xx - (x + size * .52f), yy - (y + size * .43f)) - size * .21f;
                float e = hypotf(xx - (x + size * .73f), yy - (y + size * .56f)) - size * .14f;
                float base = (hypotf((xx - (x + size * .54f)) / (size * .30f),
                                     (yy - (y + size * .58f)) / (size * .14f)) - 1) * size * .14f;
                float dist = fminf(fminf(a, b), fminf(e, base));
                if (dist <= .5f)
                    blend_pixel(xx, yy, .5f - dist, 255);
                if (dist >= -t)
                    blend_pixel(xx, yy, fminf(.5f - dist, dist + t + .5f), 0);
            }
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
    control_surface(28,178,544,232,RADIUS_LARGE,245);
    weather_symbol(48, 185, is_day == 0 && code <= 1 ? -1 : (int)code, 130);
    char b[120];
    snprintf(b, sizeof(b), "%.0f °C", temp);
    drawtext(200, 194, b, ui_font(UI_DISPLAY), 338);
    snprintf(b, sizeof(b), "%s %.0f°  ·  %.0f km/h", tr("Gefühlt"), feel, wind);
    drawtext(200, 265, b, ui_font(UI_BODY), 338);
    int rises = json_get(&j, daily, "sunrise"), sets = json_get(&j, daily, "sunset"),
        daylight = json_get(&j, daily, "daylight_duration");
    char rise[64], set[64];
    json_string(&j, json_at(&j, rises, 0), rise, sizeof(rise));
    json_string(&j, json_at(&j, sets, 0), set, sizeof(set));
    double light = json_number(&j, json_at(&j, daylight, 0));
    snprintf(b, sizeof(b), "%s %.5s  ·  %s %.5s", tr("Aufgang"),
             strlen(rise) > 11 ? rise + 11 : "--:--", tr("Untergang"),
             strlen(set) > 11 ? set + 11 : "--:--");
    divider(48, 318, 504);
    drawtext(48, 333, b, ui_font(UI_BODY), 504);
    snprintf(b, sizeof(b), "%s %.0f h %02.0f min", tr("Tageslicht"), floor(light / 3600),
             floor(fmod(light, 3600) / 60));
    drawtext(48, 367, b, ui_font(UI_SECONDARY), 504);
    drawtext(28, 429, "Vorhersage", ui_font(UI_HEADING), 544);
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
        centered(x, 473, 100, 34, b, ui_font(UI_SECONDARY));
        double c = json_number(&j, json_at(&j, codes, i));
        weather_symbol(x + 12, 505, isfinite(c) ? (int)c : 3, 80);
        snprintf(b, sizeof(b), "%.0f / %.0f°", json_number(&j, json_at(&j, high, i)),
                 json_number(&j, json_at(&j, low, i)));
        centered(x, 591, 100, 36, b, ui_font(UI_BODY));
        snprintf(b, sizeof(b), "%.0f%%", json_number(&j, json_at(&j, rain, i)));
        centered(x, 623, 100, 32, b, ui_font(UI_SECONDARY));
    }
    char stamp[64];
    json_string(&j, json_get(&j, current, "time"), stamp, sizeof(stamp));
    snprintf(b, sizeof(b), "Open-Meteo · %.16s%s", stamp, service_busy() ? " …" : "");
    drawtext(30, 697, b, ui_font(UI_SECONDARY), 540);
}
