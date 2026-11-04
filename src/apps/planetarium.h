static struct {
    double lat, lon;
    int year, month, day, hour, minute;
} sky_config;
static int sky_panel, sky_step = 0, sky_selected = -1, sky_show_lines = 1;
static unsigned char sky_cache[600 * 535];
static char sky_key[160];
static int sky_px[6000], sky_py[6000];
static astro_time_t sky_time(void) {
    return Astronomy_MakeTime(sky_config.year, sky_config.month, sky_config.day, sky_config.hour,
                              sky_config.minute, 0);
}
static int sky_project(double ra, double dec, astro_time_t *t, astro_rotation_t rotation, int *x,
                       int *y) {
    astro_spherical_t s = {.status = ASTRO_SUCCESS, .lat = dec, .lon = ra * 15, .dist = 1};
    astro_vector_t v = Astronomy_RotateVector(rotation, Astronomy_VectorFromSphere(s, *t));
    astro_equatorial_t eq = Astronomy_EquatorFromVector(v);
    astro_horizon_t h =
        Astronomy_Horizon(t, Astronomy_MakeObserver(sky_config.lat, sky_config.lon, 500), eq.ra,
                          eq.dec, REFRACTION_NORMAL);
    if (h.altitude < 0)
        return 0;
    double r = (90 - h.altitude) / 90 * 251, a = h.azimuth * .01745329252;
    *x = 300 - r * sin(a);
    *y = 425 - r * cos(a);
    return 1;
}
static void sky_now(void) {
    time_t n = time(NULL);
    struct tm t;
    gmtime_r(&n, &t);
    sky_config.year = t.tm_year + 1900;
    sky_config.month = t.tm_mon + 1;
    sky_config.day = t.tm_mday;
    sky_config.hour = t.tm_hour;
    sky_config.minute = t.tm_min;
    sky_key[0] = 0;
}
static void sky_init(void) {
    sky_config.lat = 47.856;
    sky_config.lon = 12.128;
    sky_now();
    double p[2];
    if (load_blob("sky-location-1.bin", p, sizeof(p)) && isfinite(p[0]) && isfinite(p[1]) &&
        fabs(p[0]) <= 90 && fabs(p[1]) <= 180) {
        sky_config.lat = p[0];
        sky_config.lon = p[1];
    }
}
static void sky_map(void) {
    char key[160];
    snprintf(key, sizeof(key), "%d-%d-%d-%d-%d %.4f %.4f %d", sky_config.year, sky_config.month,
             sky_config.day, sky_config.hour, sky_config.minute, sky_config.lat, sky_config.lon,
             sky_show_lines);
    astro_time_t t = sky_time();
    astro_rotation_t rot = Astronomy_Rotation_EQJ_EQD(&t);
    astro_observer_t obs = Astronomy_MakeObserver(sky_config.lat, sky_config.lon, 500);
    if (strcmp(key, sky_key)) {
        rect(0, 158, 600, 535, 255);
        ring(300, 425, 251, 2);
        ring(300, 425, 126, 1);
        for (int a = 0; a < 360; a += 45) {
            double v = a * .0174533;
            stroke(300 + 242 * sin(v), 425 + 242 * cos(v), 300 + 250 * sin(v), 425 + 250 * cos(v),
                   2);
        }
        if (sky_show_lines) {
            ink_color = 150;
            for (unsigned i = 0; i < sizeof(sky_lines) / sizeof(*sky_lines); i++) {
                int x, y, u, v;
                if (sky_project(sky_lines[i][0], sky_lines[i][1], &t, rot, &x, &y) &&
                    sky_project(sky_lines[i][2], sky_lines[i][3], &t, rot, &u, &v) &&
                    abs(u - x) + abs(v - y) < 300)
                    stroke(x, y, u, v, 1);
            }
            ink_color = 0;
        }
        for (int yy = 158; yy < 693; yy++)
            for (int xx = 0; xx < 600; xx++) {
                int at = yy * 600 + xx;
                if (ui_pixels[at] < 255)
                    ui_pixels[at] = 145 + ui_pixels[at] * 110 / 255;
            }
        ring(300, 425, 251, 2);
        for (unsigned i = 0; i < sizeof(sky_stars) / sizeof(*sky_stars); i++) {
            sky_px[i] = sky_py[i] = -1000;
            int x, y;
            if (sky_stars[i].mag <= 5.5 &&
                sky_project(sky_stars[i].ra, sky_stars[i].dec, &t, rot, &x, &y)) {
                sky_px[i] = x;
                sky_py[i] = y;
                disc(x, y, sky_stars[i].mag < 1 ? 3 : sky_stars[i].mag < 3 ? 2 : 1, 0);
                if (sky_stars[i].mag < 1.1 && *sky_stars[i].name)
                    drawtext_raw(x + 6, y - 8, sky_stars[i].name, &caption, 140);
            }
        }
        for (int i = 0; i < 535; i++)
            memcpy(sky_cache + i * 600, ui_pixels + (i + 158) * 600, 600);
        snprintf(sky_key, sizeof(sky_key), "%s", key);
    } else
        for (int i = 0; i < 535; i++)
            memcpy(ui_pixels + (i + 158) * 600, sky_cache + i * 600, 600);
    const astro_body_t planets[] = {BODY_SUN,  BODY_MOON,    BODY_MERCURY, BODY_VENUS,
                                    BODY_MARS, BODY_JUPITER, BODY_SATURN};
    const char *names[] = {"Sun", "Moon", "Mercury", "Venus", "Mars", "Jupiter", "Saturn"};
    for (int i = 0; i < 7; i++) {
        astro_equatorial_t eq = Astronomy_Equator(planets[i], &t, obs, EQUATOR_OF_DATE, ABERRATION);
        if (eq.status != ASTRO_SUCCESS)
            continue;
        astro_horizon_t h = Astronomy_Horizon(&t, obs, eq.ra, eq.dec, REFRACTION_NORMAL);
        if (h.altitude >= 0) {
            double r = (90 - h.altitude) / 90 * 251, a = h.azimuth * .01745329252;
            int x = 300 - r * sin(a), y = 425 - r * cos(a);
            disc(x, y, 6, 255);
            ring(x, y, 6, 2);
            drawtext_raw(x + 8, y - 11, tr(names[i]), &small, 130);
        }
    }
    centered(280, 158, 40, 28, "N", &caption);
    centered(280, 666, 40, 28, "S", &caption);
    drawtext_raw(18, 413, "E", &caption, 30);
    drawtext_raw(563, 413, "W", &caption, 30);
    if (sky_selected >= 0) {
        rect(45, 588, 510, 80, 255);
        key_outline(45, 588, 510, 80);
        const char *n = sky_stars[sky_selected].name;
        astro_constellation_t c =
            Astronomy_Constellation(sky_stars[sky_selected].ra, sky_stars[sky_selected].dec);
        char b[200];
        snprintf(b, 200, "%s · %s", *n ? n : "Star", c.status == ASTRO_SUCCESS ? c.name : "");
        drawtext_raw(58, 599, b, &small, 484);
        snprintf(b, 200, "RA %.2fh · Dec %.1f° · %.1f mag", sky_stars[sky_selected].ra,
                 sky_stars[sky_selected].dec, sky_stars[sky_selected].mag);
        drawtext_raw(58, 631, b, &caption, 484);
    }
}
static void sky_draw(void) {
    heading_width("Planetarium", 214);
    icon_button(528, 74, IC_SETTINGS);
    char b[150];
    snprintf(b, sizeof(b), "%02d.%02d.%04d %02d:%02d UTC · %.2f°, %.2f°", sky_config.day,
             sky_config.month, sky_config.year, sky_config.hour, sky_config.minute, sky_config.lat,
             sky_config.lon);
    centered(20, 124, 560, 31, b, &caption);
    sky_map();
    const char *steps[] = {"+1 h", "+1 d", "+1 y", "+1000 y"};
    segments(83, 696, 434, 38, steps, 4, sky_step);
    icon_button(24, 696, IC_PREV);
    icon_button(530, 696, IC_NEXT);
    button_h(28, 747, 162, 42, "Jetzt", 0);
    button_h(210, 747, 170, 42, "Ereignisse", 0);
    button_h(400, 747, 172, 42, "Datum", 0);
}
static void sky_time_label(astro_time_t t, char *out, int cap) {
    astro_utc_t u = Astronomy_UtcFromTime(t);
    snprintf(out, cap, "%02d.%02d.%04d %02d:%02d UTC", u.day, u.month, u.year, u.hour, u.minute);
}
static void sky_settings_draw(void) {
    heading("Himmel & Ort");
    const char *tabs[] = {"Ort", "Ereignisse", "Geschichte"};
    segments(28, 136, 544, 44, tabs, 3, sky_panel);
    char b[512];
    if (sky_panel == 0) {
        snprintf(b, sizeof(b), "%.4f, %.4f", sky_config.lat, sky_config.lon);
        button_h(28, 228, 544, 56, b, 0);
        drawtext(30, 194, "Breitengrad, Längengrad", &caption, 540);
        drawtext(30, 330, "Sternbild-Linien", &body, 450);
        toggle(492, 330, sky_show_lines);
        wrapped(30, 430,
                "Tippe einen Stern an, um Namen, Sternbild und Koordinaten zu sehen. Datum und "
                "Uhrzeit sind UTC. Die Karte wird vollständig lokal berechnet.",
                &small, 540, 5);
        wrapped(30, 610,
                "Zeitreise: 1 bis 12026. In ferner Zukunft ist die Sternkarte eine Näherung ohne "
                "Eigenbewegung der Sterne.",
                &caption, 540, 4);
    } else if (sky_panel == 1) {
        astro_time_t t = sky_time();
        astro_angle_result_t phase = Astronomy_MoonPhase(t);
        snprintf(b, sizeof(b), "%s: %.0f%%", tr("Mondbeleuchtung"),
                 phase.status == ASTRO_SUCCESS ? 50 * (1 - cos(phase.angle * .01745329252)) : 0);
        drawtext_raw(30, 219, b, &body, 540);
        for (int i = 0; i < 4; i++) {
            double angle = i * 90;
            astro_search_result_t r = Astronomy_SearchMoonPhase(angle, t, 40);
            const char *name[] = {"Neumond", "Erstes Viertel", "Vollmond", "Letztes Viertel"};
            drawtext(30, 290 + i * 87, name[i], &body, 540);
            if (r.status == ASTRO_SUCCESS) {
                sky_time_label(r.time, b, sizeof(b));
                drawtext_raw(30, 326 + i * 87, b, &small, 540);
            }
        }
        astro_search_result_t rise = Astronomy_SearchRiseSet(
            BODY_SUN, Astronomy_MakeObserver(sky_config.lat, sky_config.lon, 500), DIRECTION_RISE,
            t, 2);
        if (rise.status == ASTRO_SUCCESS) {
            sky_time_label(rise.time, b, sizeof(b));
            drawtext(30, 650, "Nächster Sonnenaufgang", &caption, 540);
            drawtext_raw(30, 682, b, &small, 540);
        }
    } else {
        static const char *history[] = {
            "1543 · Copernicus\nDe revolutionibus describes a heliocentric model: Earth is a "
            "planet orbiting the Sun.",
            "1609 · Galileo\nA telescope reveals lunar mountains and resolves the Milky Way into "
            "countless stars. In 1610 he observes Jupiter's four large moons.",
            "1687 · Newton\nUniversal gravitation connects falling objects on Earth with the "
            "motion of planets.",
            "1915 · Einstein\nGeneral relativity explains gravity as spacetime curvature, "
            "including Mercury's orbital precession.",
            "1995 · 51 Pegasi b\nMayor and Queloz announce a planet orbiting a Sun-like star. "
            "Thousands of exoplanets follow."};
        wrapped_raw(30, 230, history[v1_page % 5], &editorial, 540, 9);
        pager(708, &v1_page, 5);
    }
}
static void sky_shift(int sign) {
    if (sky_step == 3 || sky_step == 2) {
        sky_config.year += sign * (sky_step == 3 ? 1000 : 1);
        if (sky_config.year < 1)
            sky_config.year = 1;
        if (sky_config.year > 12026)
            sky_config.year = 12026;
        if (sky_config.day > calendar_days(sky_config.year, sky_config.month))
            sky_config.day = calendar_days(sky_config.year, sky_config.month);
    } else {
        astro_time_t t = Astronomy_AddDays(sky_time(), sign * (sky_step == 0 ? 1. / 24 : 1));
        astro_utc_t u = Astronomy_UtcFromTime(t);
        if (u.year >= 1 && u.year <= 12026) {
            sky_config.year = u.year;
            sky_config.month = u.month;
            sky_config.day = u.day;
            sky_config.hour = u.hour;
            sky_config.minute = u.minute;
        }
    }
    sky_key[0] = 0;
    sky_selected = -1;
}
