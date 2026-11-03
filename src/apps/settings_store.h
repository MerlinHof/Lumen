/* Settings/store application views and events. */
static void connect_screen(void) {
    heading("Bücher übertragen");
    int active = connect_running();
    drawtext(30, 150, "Transfer-Server", &body, 400);
    toggle(492, 150, active);
    rect(30, 207, 542, 2, 190);
    if (active) {
        const char *address = connect_address();
        char seen[2048];
        snprintf(seen, sizeof(seen), "%s/connect-peer16.txt", data_root);
        drawtext(30, 218,
                 access(seen, F_OK) ? "Bereit · im selben WLAN öffnen"
                                    : "Verbunden · Zugriff bestätigt",
                 &small, 542);
        drawtext(30, 258, "ADRESSE", &caption, 542);
        wrapped_raw(30, 291, *address ? address : tr("Keine WLAN-Adresse"), &body, 542, 2);
        if (*address) {
            char url[256];
            snprintf(url, sizeof(url), "%s/#%s", address, connect_code());
            uint8_t qr[qrcodegen_BUFFER_LEN_FOR_VERSION(10)],
                tmp[qrcodegen_BUFFER_LEN_FOR_VERSION(10)];
            if (qrcodegen_encodeText(url, tmp, qr, qrcodegen_Ecc_MEDIUM, 1, 10, qrcodegen_Mask_AUTO,
                                     true)) {
                int n = qrcodegen_getSize(qr), step = 240 / (n + 8), side = (n + 8) * step,
                    x = 300 - side / 2;
                for (int yy = 0; yy < n; yy++)
                    for (int xx = 0; xx < n; xx++)
                        if (qrcodegen_getModule(qr, xx, yy))
                            rect(x + (xx + 4) * step, 365 + (yy + 4) * step, step, step, 0);
            }
        }
        centered(28, 596, 544, 35, "Verbindungscode", &caption);
        char code[80];
        snprintf(code, sizeof(code), "%s", connect_code());
        centered(28, 630, 544, 62, code, &title);
        char timeout[120];
        snprintf(timeout, sizeof(timeout), tr("Automatisches Ende nach %d min ohne Zugriff"),
                 options[OPT_TIMEOUT]);
        centered(28, 696, 544, 30, timeout, &caption);
    } else {
        wrapped(30, 244,
                "Bücher im Browser übertragen. Aktiviere den Server und scanne den QR-Code mit "
                "einem Gerät im selben WLAN.",
                &editorial, 542, 5);
        wrapped(30, 484,
                "Der Server pausiert im Ruhemodus. Beim nächsten Start wird die aktuelle "
                "WLAN-Adresse angezeigt.",
                &small, 542, 4);
    }
    icon_button(528, 76, IC_SETTINGS);
}
static void connect_input(const Input *in) {
    if (in->kind != INPUT_TAP)
        return;
    if (in->x >= 516 && in->y >= 72 && in->y < 124) {
        if (history_count < 64)
            history[history_count++] = 8;
        setting_category = 4;
        route_to(9);
        return;
    }
    if (in->y >= 142 && in->y < 198) {
        if (connect_running()) {
            connect_stop();
            refresh_library();
        } else if (!connect_start(options[OPT_PORT], options[OPT_TIMEOUT]))
            ui_message(connect_error());
    }
}
static const char *categories[] = {"Schrift & Satz",    "Darstellung", "Bibliothek",
                                   "Blättern & Gesten", "Netzwerk",    "System"};
static const char *fonts[] = {"Charis", "Sans Serif", "Monospace", "Times", "Noto Serif"};
static const char *alignments[] = {"Linksbündig", "Blocksatz"};
static const char *tapzones[] = {"Links / rechts", "Mehr Platz vorwärts"};
static const char *lockmodes[] = {"Buchcover", "Ruhiger Text", "Uhrzeit", "Eigenes Bild",
                                  "Fotos · stündlich"};
static const char *languages[] = {"Deutsch", "English", "Français"};
typedef struct {
    const char *label;
    int category, key, min, max, step;
    const char **choices;
    int count;
} SettingRow;
static SettingRow setting_rows[] = {
    {"Schriftfamilie", 0, OPT_FONT, 0, 4, 1, fonts, 5},
    {"Ausrichtung", 0, OPT_ALIGN, 0, 1, 1, alignments, 2},
    {"Zeichenabstand", 0, OPT_LETTERSPACE, 0, 20, 2, NULL, 0},
    {"Absatzabstand", 0, OPT_PARAGRAPH, 0, 24, 4, NULL, 0},
    {"Sprache / Language", 1, OPT_LANGUAGE, 0, 2, 1, languages, 3},
    {"Tastatursprache", 1, OPT_KEYBOARD_LANGUAGE, 0, 2, 1, languages, 3},
    {"Datum anzeigen", 1, OPT_DATE, 0, 1, 1, NULL, 0},
    {"Uhrzeit anzeigen", 1, OPT_CLOCK, 0, 1, 1, NULL, 0},
    {"WLAN-Status anzeigen", 1, OPT_WIFI, 0, 1, 1, NULL, 0},
    {"Akkusymbol", 1, OPT_BATTERY, 0, 1, 1, NULL, 0},
    {"Akku-Prozentzahl", 1, OPT_BATTERY_PERCENT, 0, 1, 1, NULL, 0},
    {"Invertierte Darstellung", 1, OPT_INVERT, 0, 1, 1, NULL, 0},
    {"Apps-Tab", 1, OPT_APPS, 0, 1, 1, NULL, 0},
    {"Tab-Beschriftungen", 1, OPT_TAB_LABELS, 0, 1, 1, NULL, 0},
    {"Cover zuschneiden", 1, OPT_COVER_CROP, 0, 1, 1, NULL, 0},
    {"Runde Cover-Ecken", 1, OPT_COVER_RADIUS, 0, 1, 1, NULL, 0},
    {"Lesefortschritt", 1, OPT_PROGRESS, 0, 1, 1, NULL, 0},
    {"Sperrbild", 1, OPT_LOCK, 0, 4, 1, lockmodes, 5},
    {"Sperrbild-Hinweis", 1, OPT_LOCK_HINT, 0, 1, 1, NULL, 0},
    {"Weicher Cover-Hintergrund", 1, OPT_LOCK_BLUR, 0, 1, 1, NULL, 0},
    {"Wischen zum Blättern", 3, OPT_SWIPE, 0, 1, 1, NULL, 0},
    {"Tippzonen", 3, OPT_TAP, 0, 1, 1, tapzones, 2},
    {"Connect-Port", 4, OPT_PORT, 1024, 65535, 1, NULL, 0},
    {"Inaktivität (Minuten)", 4, OPT_TIMEOUT, 1, 60, 1, NULL, 0},
};
static const char *settings_sections[] = {"WLAN",       "Bücher übertragen", "Lesen", "Darstellung",
                                          "Bibliothek", "Datum & Uhrzeit",   "System"};
static const char *settings_hints[] = {
    "Netzwerke und Verbindung",           "Webserver, Adresse und Zugriff",
    "Schrift, Satz und Gesten",           "Sprache, Sperrbild und Statusleiste",
    "Lesestände und Verwaltung",          "Zeit, Zeitzone und WLAN-Abgleich",
    "Speicher, Version und Konfiguration"};
static void settings_home(void) {
    heading("Einstellungen");
    for (int i = 0; i < 7; i++) {
        int y = 134 + i * 80;
        region(28, y, 544, 69);
        drawtext(30, y + 4, settings_sections[i], &body, 480);
        drawtext(30, y + 42, settings_hints[i], &caption, 480);
        icon(526, y + 13, IC_NEXT);
        if (i < 6)
            rect(28, y + 75, 544, 2, 190);
    }
}
static void open_store(void) {
    route_to(10);
    store_home = !*store_query;
    if (!opds_count) {
        char path[2048];
        snprintf(path, sizeof(path), tr("%s/../catalogs/gutenberg-de.opds"), data_root);
        discover_parse(path, opds_source);
    }
#ifndef LUMEN_HOST
    if (!discover_busy())
        store_search();
#else
    if (getenv("LUMEN_STORE_NETWORK") && !discover_busy())
        store_search();
#endif
}
static const char *sort_modes[] = {"Titel",           "Autor",      "Zuletzt gelesen",
                                   "Neu hinzugefügt", "Dateigröße", "Fortschritt"};
static void more_settings(void) {
    heading(categories[setting_category]);
    if (setting_category == 1)
        drawtext(30, 121, setting_page ? "Bücher & Sperrbild" : "Sprache & Statusleiste", &caption,
                 540);
    if (setting_category == 2) {
        char b[100];
        snprintf(b, sizeof(b), tr("%d Bücher in deiner Bibliothek"), library_count);
        drawtext(30, 145, b, &editorial, 540);
        wrapped(30, 197, "Lesestände werden automatisch gespeichert.", &caption, 540, 2);
        select_row17(279, "Sortierung", sort_modes[settings.sort % 6]);
        rect(28, 411, 544, 2, 190);
        button_h(28, 449, 544, 56, "Bibliothek neu einlesen", 0);
        button_h(28, 527, 544, 56, "Buchcover neu einlesen", 0);
        return;
    }
    if (setting_category == 3)
        u17_sleep_row(590);
    int row = 0;
    for (size_t i = 0; i < sizeof(setting_rows) / sizeof(*setting_rows); i++) {
        SettingRow *r = &setting_rows[i];
        if (r->category != setting_category)
            continue;
        int index = row++;
        if (setting_category == 1 && index / 8 != setting_page)
            continue;
        int y = (setting_category == 1 ? 155 : 235) +
                (setting_category == 1 ? index % 8 : index) * (setting_category == 1 ? 62 : 91);
        drawtext(30, y, r->label, &small, r->choices ? 310 : 390);
        char value[80];
        int v = options[r->key];
        if (r->choices) {
            snprintf(value, sizeof(value), tr("%s"), r->choices[v >= 0 && v < r->count ? v : 0]);
            dropdown_button(360, y - 4, 210, 48, value);
        } else if (r->min == 0 && r->max == 1) {
            toggle(492, y, v);
        } else {
            snprintf(value, sizeof(value), tr("%d"), v);
            drawtext(350, y, value, &body, 80);
            button(428, y - 4, 62, "-", 0);
            button(505, y - 4, 62, "+", 0);
        }
    }
    if (setting_category == 1) {
        pager(679, &setting_page, (row + 7) / 8);
    }
    if (setting_category == 4)
        wrapped(30, 580, "Änderungen gelten beim nächsten Serverstart.", &small, 540, 2);
}
static void settings_input(const Input *in) {
    if (in->kind != INPUT_TAP)
        return;
    int x = in->x, y = in->y;
    if (setting_category == 1 && y >= 679 && y < 723) {
        int count = 0;
        for (size_t i = 0; i < sizeof(setting_rows) / sizeof(*setting_rows); i++)
            if (setting_rows[i].category == 1)
                count++;
        if (x < 300 && setting_page > 0)
            setting_page--;
        else if (x >= 300 && (setting_page + 1) * 8 < count)
            setting_page++;
        return;
    }
    if (setting_category == 5 && y >= 145 && y < 193) {
        open_keyboard(4, "Store-Katalogadresse", opds_source);
        return;
    }
    int row = 0;
    for (size_t i = 0; i < sizeof(setting_rows) / sizeof(*setting_rows); i++) {
        SettingRow *r = &setting_rows[i];
        if (r->category != setting_category)
            continue;
        int index = row++;
        if (setting_category == 1 && index / 8 != setting_page)
            continue;
        int top = (setting_category == 1 ? 151 : 231) +
                  (setting_category == 1 ? index % 8 : index) * (setting_category == 1 ? 62 : 91);
        if (y < top || y >= top + 48)
            continue;
        if (r->choices) {
            open_dropdown(100 + r->key, r->choices, r->count, options[r->key], 220, top + 50, 350);
            return;
        }
        if (r->max == 1) {
            options[r->key] = !options[r->key];
            if (r->key == OPT_AUTOSTART) {
                char path[2048];
                snprintf(path, sizeof(path), tr("%s/../DISABLE_AUTOSTART"), data_root);
                if (options[r->key])
                    unlink(path);
                else
                    atomic_write(path, "", 0);
            }
        } else {
            options[r->key] += x >= 505 ? r->step : x >= 428 ? -r->step : 0;
            if (options[r->key] < r->min)
                options[r->key] = r->min;
            if (options[r->key] > r->max)
                options[r->key] = r->max;
        }
        library_settings_save();
        engine_options(options[OPT_FONT], options[OPT_ALIGN], options[OPT_LETTERSPACE],
                       options[OPT_PARAGRAPH]);
        return;
    }
    if (setting_category == 2) {
        if (x >= 300 && y >= 279 && y < 327) {
            open_dropdown(900, sort_modes, 6, settings.sort, 300, 332, 272);
            return;
        }
        if (y >= 449 && y < 505) {
            refresh_library();
            ui_message("Bibliothek aktualisiert.");
        } else if (y >= 527 && y < 583) {
            for (int i = 0; i < library_count; i++) {
                char path[2048];
                library_cover_path(&library[i], path, sizeof(path));
                unlink(path);
            }
            refresh_library();
            ui_message("Buchcover neu eingelesen.");
        }
    }
}
static void store_cover(OPDSItem *item, int x, int y, int w, int h) {
    const char *id = strstr(item->url, "/ebooks/");
    int skip = 8;
    if (!id) {
        id = strstr(item->url, "/files/");
        skip = 7;
    }
    if (!id) {
        id = strstr(item->url, "/cache/epub/");
        skip = 12;
    }
    Book b = {0};
    b.id = 0x7000000000000000ULL + (id ? strtoull(id + skip, NULL, 10) : 0);
    snprintf(b.title, sizeof(b.title), tr("%s"), item->title);
    snprintf(b.format, sizeof(b.format), tr("%s"), item->format);
    cover_size(&b, x, y, w, h);
}
static void store_restore(void) {
    memcpy(opds_items, store_results, store_total * sizeof(*opds_items));
    opds_count = store_total;
    opds_page = 0;
    store_federated = 1;
}
static int store_complete(int done) {
    if (done > 0) {
        int added = 0, budget = OPDS_LIMIT / (store_provider_count ? store_provider_count : 1);
        for (int i = 0; i < opds_count && store_total < OPDS_LIMIT && added < budget; i++) {
            OPDSItem *item = &opds_items[i];
            if (store_stage >= 3 && *store_query && !strcasestr(item->title, store_query) &&
                !strcasestr(item->author, store_query))
                continue;
            int duplicate = 0;
            for (int k = 0; k < store_total; k++)
                if (!strcmp(store_results[k].url, item->url) ||
                    (!strcmp(store_results[k].title, item->title) &&
                     !strcmp(store_results[k].author, item->author))) {
                    duplicate = 1;
                    break;
                }
            if (!duplicate) {
                store_results[store_total++] = *item;
                added++;
            }
        }
    }
    store_stage++;
    if (store_stage <= store_provider_count) {
        store_restore();
        if (discover_start(store_urls[store_stage - 1], NULL))
            return 1;
        return store_complete(-1);
    }
    store_stage = 0;
    store_restore();
    if (!opds_count)
        ui_message("Keine Treffer. Suche oder Verbindung prüfen.");
    return 1;
}
static void store_search(void) {
    char encoded[1200];
    size_t n = 0;
    store_home = !*store_query;
    const char *term = *store_query ? store_query : "books";
    for (const unsigned char *p = (const unsigned char *)term; *p && n + 4 < sizeof(encoded); p++) {
        if (isalnum(*p) && *p < 128)
            encoded[n++] = *p;
        else {
            snprintf(encoded + n, 4, "%%%02X", *p);
            n += 3;
        }
    }
    encoded[n] = 0;
    snprintf(store_urls[0], sizeof(store_urls[0]),
             "https://www.gutenberg.org/ebooks/search.opds/?query=%s&sort_order=downloads",
             *store_query                 ? encoded
             : options[OPT_LANGUAGE] == 0 ? "l.de"
                                          : "");
    snprintf(store_urls[1], sizeof(store_urls[1]),
             "https://archive.org/"
             "advancedsearch.php?q=mediatype%%3Atexts%%20AND%%20%%28licenseurl%%3A%%"
             "28creativecommons.org%%2A%%29%%20OR%%20collection%%3Agutenberg%%29%%20AND%%20%%28%s%%"
             "29&fl%%5B%%5D=identifier&fl%%5B%%5D=title&fl%%5B%%5D=creator&rows=50&output=xml",
             encoded);
    u15_sources_prepare(encoded);
    discover_cancel();
    store_total = 0;
    store_federated = 1;
    store_stage = 1;
    opds_page = opds_depth = 0;
    opds_count = 0;
    if (!store_provider_count) {
        store_stage = 0;
        ui_message("Quellen in den Store-Einstellungen aktivieren.");
        return;
    }
    if (!discover_start(store_urls[0], NULL))
        store_complete(-1);
}
static void discover_screen(void) {
    heading("Store");
    icon_button(468, 74, IC_SETTINGS);
    search_action();
    drawtext(30, 133,
             *store_query  ? store_query
             : store_stage ? "Mehrere Kataloge werden durchsucht …"
                           : "Gutenberg · Internet Archive",
             &caption, 540);
    if (store_home) {
        static int last_count = -1;
        static unsigned seed;
        static int visit = -1;
        int *selected = store15_grid;
        if (last_count != opds_count || (unsigned)visit != runtime.generation) {
            last_count = opds_count;
            visit = runtime.generation;
            seed = (unsigned)time(NULL) ^ runtime.generation;
            for (int i = 0; i < 4; i++) {
                selected[i] = -1;
                if (opds_count) {
                    for (int tries = 0; tries < 100; tries++) {
                        seed = seed * 1664525u + 1013904223u;
                        int candidate = seed % opds_count, unique = 1;
                        for (int j = 0; j < i; j++)
                            if (selected[j] == candidate)
                                unique = 0;
                        if (unique) {
                            selected[i] = candidate;
                            break;
                        }
                    }
                }
            }
        }
        for (int i = 0; i < 4; i++)
            if (selected[i] >= 0) {
                int x = 36 + (i % 2) * 284, y = 174 + (i / 2) * 250;
                OPDSItem *b = &opds_items[selected[i]];
                store_cover(b, x + 57, y, 130, 178);
                wrapped_raw(x, y + 185, b->title, &small, 244, 2);
                region(x, y, 244, 235);
            }
        return;
    }
    for (int row = 0; row < 5 && opds_page * 5 + row < opds_count; row++) {
        OPDSItem *b = &opds_items[opds_page * 5 + row];
        int y = 174 + row * 87;
        region(28, y, 544, 88);
        store_cover(b, 30, y + 3, 54, 75);
        char authorbuf[256];
        const char *author = *b->author
                                 ? author_label(b->author, authorbuf, sizeof(authorbuf))
                                 : tr(b->acquisition ? "EPUB herunterladen" : "Katalog öffnen");
        int title_lines = textwidth_raw(b->title, &small) > 412 ? 2 : 1;
        wrapped_raw(100, y + 1, b->title, &small, 412, 2);
        drawtext_raw(100, y + 5 + title_lines * (small.h - 4), author, &caption, 412);
        icon(530, y + 19, IC_NEXT);
        if (row < 4)
            rect(108, y + 87, 464, 2, 190);
    }
    if (!opds_count)
        wrapped(28, 230,
                discover_busy()
                    ? "Der Katalog wird geladen …"
                    : "Kein Katalog geladen. WLAN in den Einstellungen verbinden und erneut laden.",
                &editorial, 544, 4);
    pager(679, &opds_page, (opds_count + 4) / 5);
}
static void discover_detail(void) {
    if (opds_selected < 0 || opds_selected >= opds_count)
        return;
    OPDSItem *b = &opds_items[opds_selected];
    heading("Buch entdecken");
    store_cover(b, 28, 145, 120, 164);
    wrapped_raw(176, 145, b->title, &editorial, 396, 3);
    wrapped_raw(176, 280, b->author, &small, 396, 2);
    wrapped_raw(28, 366, b->summary, &small, 544, 8);
    button(28, 665, 544, discover_busy() ? "Download abbrechen" : "Buch herunterladen", 0);
}
static void discover_input(const Input *in) {
    if (in->kind != INPUT_TAP)
        return;
    int x = in->x, y = in->y;
    if (screen == 10) {
        if (search_action_hit(x, y)) {
            open_keyboard(6, "Bücher im Store suchen", store_query);

        } else if ((store_home && y >= 174 && y < 659) || (!store_home && y >= 174 && y < 609)) {
            int i = store_home ? store15_grid[((y - 174) / 250) * 2 + (x >= 300)]
                               : opds_page * 5 + (y - 174) / 87;
            if (i >= 0 && i < opds_count) {
                discover_cancel();
                store_stage = 0;
                if (opds_items[i].acquisition) {
                    opds_selected = i;
                    if (history_count < 64)
                        history[history_count++] = 10;
                    route_to(11);
                } else {
                    if (opds_depth < 16)
                        snprintf(opds_history[opds_depth++], 2048, "%s",
                                 store_federated ? "lumen:search" : discover_url());
                    store_federated = 0;
                    if (!discover_start(opds_items[i].url, NULL))
                        ui_message(discover_error());
                }
            }
        } else if (y >= 679 && y < 723) {
            if (x < 300 && opds_page)
                opds_page--;
            else if (x >= 300 && (opds_page + 1) * 5 < opds_count)
                opds_page++;
        }
    } else if (y >= 665 && y < 713) {
        if (discover_busy())
            discover_cancel();
        else if (opds_selected >= 0 && opds_selected < opds_count &&
                 !discover_start(opds_items[opds_selected].url, &opds_items[opds_selected]))
            ui_message(discover_error());
    }
}

void ui_loading(const char *message) {
    widget_count = 0;
    memset(ui_pixels, 255, sizeof(ui_pixels));
    int w = textwidth("LUMEN", &title);
    drawtext((600 - w) / 2, 285, "LUMEN", &title, 560);
    w = textwidth(message, &small);
    drawtext(w < 560 ? (600 - w) / 2 : 20, 370, message, &small, 560);
}

static void wifi_open(void) {
    wifi_page = 0;
    wifi_exit = 0;
    wifi_enabled = device_wifi_enabled();
    network_status(wifi_ssid, sizeof(wifi_ssid));
    if (*wifi_ssid)
        wifi_enabled = 1;
    if (wifi_enabled) {
        wifi_scanning = network_scan();
        network_results();
    } else
        wifi_count = 0;
}
static void wifi_screen(void) {
    heading("WLAN");
    drawtext(28, 145, "WLAN", &body, 420);
    toggle(492, 145, wifi_enabled);
    rect(28, 209, 544, 2, 190);
    if (!wifi_enabled) {
        wrapped(28, 258, "WLAN ist ausgeschaltet.", &editorial, 544, 2);
        return;
    }
    for (int r = 0; r < 6 && wifi_page * 6 + r < wifi_count; r++) {
        WifiNetwork *n = &wifi_networks[wifi_page * 6 + r];
        int y = 228 + r * 72;
        region(28, y, 470, 70);
        int connected = *wifi_ssid && !strcmp(n->ssid, wifi_ssid);
        drawtext_raw(connected ? 61 : 30, y + 1, n->ssid, &small, connected ? 423 : 454);
        if (connected) {
            stroke(31, y + 17, 38, y + 24, 2);
            stroke(38, y + 24, 51, y + 7, 2);
        }
        drawtext(30, y + 35,
                 connected                 ? "Verbunden"
                 : n->saved                ? "Gespeichert"
                 : strstr(n->flags, "WPA") ? "Passwort geschützt"
                                           : "Offenes Netz",
                 &caption, 430);
        if (n->saved)
            icon_button(522, y + 10, IC_TRASH);
        if (r < 5)
            rect(28, y + 70, 544, 2, 190);
    }
    if (!wifi_count)
        wrapped(28, 258, wifi_scanning ? "Suche läuft …" : "Keine Netzwerke gefunden.", &editorial,
                544, 3);
    pager(679, &wifi_page, (wifi_count + 5) / 6);
    icon_button(528, 679, IC_RESTART);
}
static void wifi_input(const Input *in) {
    if (in->kind != INPUT_TAP)
        return;
    int x = in->x, y = in->y;
    if (y >= 137 && y < 193) {
        int enabled = !wifi_enabled;
        if (!device_wifi(enabled)) {
            ui_message("WLAN konnte nicht geändert werden.");
            return;
        }
        wifi_enabled = enabled;
        if (!enabled) {
            connect_stop();
            discover_cancel();
            network_disconnect();
            wifi_ssid[0] = 0;
            wifi_count = 0;
            wifi_scanning = 0;
        } else {
            wifi_scanning = network_scan();
            network_results();
        }
        return;
    }
    if (!wifi_enabled)
        return;
    if (y >= 228 && y < 660) {
        int i = wifi_page * 6 + (y - 228) / 72;
        if (i >= wifi_count)
            return;
        WifiNetwork *n = &wifi_networks[i];
        wifi_selected = i;
        if (x >= 510 && n->saved) {
            int connected = !strcmp(n->ssid, wifi_ssid);
            if (network_forget(i)) {
                if (connected) {
                    connect_stop();
                    network_disconnect();
                    wifi_ssid[0] = 0;
                }
                ui_message("Netzwerk vergessen.");
            } else
                ui_message(network_error());
        } else if (x < 510) {
            if (n->saved || !strstr(n->flags, "WPA"))
                ui_message(network_connect(i, "") ? "Verbindung wird hergestellt …"
                                                  : network_error());
            else
                open_keyboard(5, n->ssid, "");
        }
    } else if (y >= 679 && y < 723) {
        if (x >= 430) {
            wifi_scanning = network_scan();
            if (!wifi_scanning)
                ui_message(network_error());
        } else if (x < 300 && wifi_page)
            wifi_page--;
        else if (x >= 300 && (wifi_page + 1) * 6 < wifi_count)
            wifi_page++;
    }
}
