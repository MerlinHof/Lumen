/* App-owned module; assembled by apps/host/compose.h. */
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
    memcpy(opds_items, store_ui.store_results, store_ui.store_total * sizeof(*opds_items));
    opds_count = store_ui.store_total;
    store_ui.opds_page = 0;
    store_ui.store_federated = 1;
}
static int store_complete(int done) {
    if (done > 0) {
        int added = 0, budget = OPDS_LIMIT / (store_ui.store_provider_count ? store_ui.store_provider_count : 1);
        for (int i = 0; i < opds_count && store_ui.store_total < OPDS_LIMIT && added < budget; i++) {
            OPDSItem *item = &opds_items[i];
            if (store_ui.store_stage >= 3 && *store_ui.store_query && !strcasestr(item->title, store_ui.store_query) &&
                !strcasestr(item->author, store_ui.store_query))
                continue;
            int duplicate = 0;
            for (int k = 0; k < store_ui.store_total; k++)
                if (!strcmp(store_ui.store_results[k].url, item->url) ||
                    (!strcmp(store_ui.store_results[k].title, item->title) &&
                     !strcmp(store_ui.store_results[k].author, item->author))) {
                    duplicate = 1;
                    break;
                }
            if (!duplicate) {
                store_ui.store_results[store_ui.store_total++] = *item;
                added++;
            }
        }
    }
    store_ui.store_stage++;
    if (store_ui.store_stage <= store_ui.store_provider_count) {
        store_restore();
        if (discover_start(store_ui.store_urls[store_ui.store_stage - 1], NULL))
            return 1;
        return store_complete(-1);
    }
    store_ui.store_stage = 0;
    store_restore();
    if (!opds_count)
        ui_message("Keine Treffer. Suche oder Verbindung prüfen.");
    return 1;
}
static void store_search(void) {
    char encoded[1200];
    size_t n = 0;
    store_ui.store_home = !*store_ui.store_query;
    const char *term = *store_ui.store_query ? store_ui.store_query : "books";
    for (const unsigned char *p = (const unsigned char *)term; *p && n + 4 < sizeof(encoded); p++) {
        if (isalnum(*p) && *p < 128)
            encoded[n++] = *p;
        else {
            snprintf(encoded + n, 4, "%%%02X", *p);
            n += 3;
        }
    }
    encoded[n] = 0;
    snprintf(store_ui.store_urls[0], sizeof(store_ui.store_urls[0]),
             "https://www.gutenberg.org/ebooks/search.opds/?query=%s&sort_order=downloads",
             *store_ui.store_query                 ? encoded
             : options[OPT_LANGUAGE] == 0 ? "l.de"
                                          : "");
    snprintf(store_ui.store_urls[1], sizeof(store_ui.store_urls[1]),
             "https://archive.org/"
             "advancedsearch.php?q=mediatype%%3Atexts%%20AND%%20%%28licenseurl%%3A%%"
             "28creativecommons.org%%2A%%29%%20OR%%20collection%%3Agutenberg%%29%%20AND%%20%%28%s%%"
             "29&fl%%5B%%5D=identifier&fl%%5B%%5D=title&fl%%5B%%5D=creator&rows=50&output=xml",
             encoded);
    u15_sources_prepare(encoded);
    discover_cancel();
    store_ui.store_total = 0;
    store_ui.store_federated = 1;
    store_ui.store_stage = 1;
    store_ui.opds_page = store_ui.opds_depth = 0;
    opds_count = 0;
    if (!store_ui.store_provider_count) {
        store_ui.store_stage = 0;
        ui_message("Quellen in den Store-Einstellungen aktivieren.");
        return;
    }
    if (!discover_start(store_ui.store_urls[0], NULL))
        store_complete(-1);
}
static void discover_screen(void) {
    heading_actions("Store", 2);
    icon_button(468, 74, IC_SETTINGS);
    search_action();
    drawtext(30, 133,
             *store_ui.store_query  ? store_ui.store_query
             : store_ui.store_stage ? "Mehrere Kataloge werden durchsucht …"
                           : "Gutenberg · Internet Archive",
             ui_font(UI_SECONDARY), 540);
    if (store_ui.store_home) {
        static int last_count = -1;
        static unsigned seed;
        static int visit = -1;
        int *selected = store_ui.store15_grid;
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
                wrapped_raw(x, y + 185, b->title, ui_font(UI_BODY), 244, 2);
                region(x, y, 244, 235);
            }
        return;
    }
    for (int row = 0; row < 5 && store_ui.opds_page * 5 + row < opds_count; row++) {
        OPDSItem *b = &opds_items[store_ui.opds_page * 5 + row];
        int y = 174 + row * 87;
        char authorbuf[256];
        const char *author = *b->author
                                 ? author_label(b->author, authorbuf, sizeof(authorbuf))
                                 : tr(b->acquisition ? "EPUB herunterladen" : "Katalog öffnen");
        region(28, y, 544, 86);
        store_cover(b, 30, y + 6, 50, 69);
        FlowLine lines[2];
        int n = text_flow(b->title, ui_font(UI_BODY), 412, lines, 2);
        ListLayout l = list_layout(98, y, 422, 87, 0, 0, n, 1, ui_font(UI_BODY));
        list_text(100, l.title_y, 412, b->title, ui_font(UI_BODY), 2);
        int old = ink_color;
        ink_color = UI_SECONDARY_INK;
        label_ellipsis(100, l.detail_y, 412, 16, author, ui_font(UI_SECONDARY), 0);
        ink_color = old;
        icon(528, l.action_y, IC_NEXT);
        divider(28, y + 86, 544);
    }
    if (!opds_count)
        wrapped(28, 230,
                discover_busy()
                    ? "Der Katalog wird geladen …"
                    : "Kein Katalog geladen. WLAN in den Einstellungen verbinden und erneut laden.",
                ui_font(UI_TITLE), 544, 4);
    pager(679, &store_ui.opds_page, (opds_count + 4) / 5);
}
static void discover_detail(void) {
    if (store_ui.opds_selected < 0 || store_ui.opds_selected >= opds_count)
        return;
    OPDSItem *b = &opds_items[store_ui.opds_selected];
    heading("Buch entdecken");
    store_cover(b, 28, 145, 120, 164);
    wrapped_raw(176, 145, b->title, ui_font(UI_TITLE), 396, 3);
    wrapped_raw(176, 280, b->author, ui_font(UI_BODY), 396, 2);
    wrapped_raw(28, 366, b->summary, ui_font(UI_BODY), 544, 8);
    button(28, 665, 544, discover_busy() ? "Download abbrechen" : "Buch herunterladen", 0);
}
static void discover_input(const Input *in) {
    if (in->kind != INPUT_TAP)
        return;
    int x = in->x, y = in->y;
    if (ui_route() == 10) {
        if (search_action_hit(x, y)) {
            open_keyboard(6, "Bücher im Store suchen", store_ui.store_query);

        } else if ((store_ui.store_home && y >= 174 && y < 659) || (!store_ui.store_home && y >= 174 && y < 609)) {
            int i = store_ui.store_home ? store_ui.store15_grid[((y - 174) / 250) * 2 + (x >= 300)]
                               : store_ui.opds_page * 5 + (y - 174) / 87;
            if (i >= 0 && i < opds_count) {
                discover_cancel();
                store_ui.store_stage = 0;
                if (opds_items[i].acquisition) {
                    store_ui.opds_selected = i;
                    if (history_count < 64)
                        history[history_count++] = 10;
                    route_to(11);
                } else {
                    if (store_ui.opds_depth < 16)
                        snprintf(store_ui.opds_history[store_ui.opds_depth++], 2048, "%s",
                                 store_ui.store_federated ? "lumen:search" : discover_url());
                    store_ui.store_federated = 0;
                    if (!discover_start(opds_items[i].url, NULL))
                        ui_message(discover_error());
                }
            }
        } else if (y >= 679 && y < 723) {
            if (x < 300 && store_ui.opds_page)
                store_ui.opds_page--;
            else if (x >= 300 && (store_ui.opds_page + 1) * 5 < opds_count)
                store_ui.opds_page++;
        }
    } else if (y >= 665 && y < 713) {
        if (discover_busy())
            discover_cancel();
        else if (store_ui.opds_selected >= 0 && store_ui.opds_selected < opds_count &&
                 !discover_start(opds_items[store_ui.opds_selected].url, &opds_items[store_ui.opds_selected]))
            ui_message(discover_error());
    }
}

void ui_loading(const char *message) {
    widget_count = 0;
    memset(ui_pixels, 255, sizeof(ui_pixels));
    int w = textwidth("LUMEN", ui_font(UI_DISPLAY));
    drawtext((600 - w) / 2, 285, "LUMEN", ui_font(UI_DISPLAY), 560);
    w = textwidth(message, ui_font(UI_BODY));
    drawtext(w < 560 ? (600 - w) / 2 : 20, 370, message, ui_font(UI_BODY), 560);
}
