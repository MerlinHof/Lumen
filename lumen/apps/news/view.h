/* App-owned module; assembled by apps/host/compose.h. */
static int press_dates[2048], press_dates_count, press_date_page, press_list_page,
    press_reader_page, press_reader_pages, press_offsets[4096], press_initial_y = 145,
                                                                press_has_image;
static uint64_t press_reader_hash;
static char press_query[256];
static int press_loading_kind;
static void press_archive_scan(void) {
    v1_archive_scan();
    press_dates_count = 0;
    for (int i = 0; i < news_date_count; i++)
        press_dates[press_dates_count++] = news_dates[i];
    char p[2048];
    local_path(p, sizeof(p), "press");
    DIR *d = opendir(p);
    if (d) {
        struct dirent *e;
        while ((e = readdir(d)) && press_dates_count < 2048) {
            int date;
            char tail;
            if (sscanf(e->d_name, "edition-%8d.bin%c", &date, &tail) == 1 &&
                strlen(e->d_name) == 20) {
                int dup = 0;
                for (int i = 0; i < press_dates_count; i++)
                    if (press_dates[i] == date)
                        dup = 1;
                if (!dup)
                    press_dates[press_dates_count++] = date;
            }
        }
        closedir(d);
    }
    for (int i = 0; i < press_dates_count; i++)
        for (int j = i + 1; j < press_dates_count; j++)
            if (press_dates[j] > press_dates[i]) {
                int t = press_dates[i];
                press_dates[i] = press_dates[j];
                press_dates[j] = t;
            }
}
static void press_legacy(int date) {
    memset(&press_list, 0, sizeof(press_list));
    press_list.magic = 0x50524c31;
    press_list.date = date;
    if (news_load(date))
        for (int i = 0; i < newspaper.count; i++) {
            NewsArticle *a = &newspaper.article[i];
            if (a->category == 5)
                continue;
            PressItem *b = &press_list.item[press_list.count++];
            snprintf(b->title, 256, "%s", a->title);
            snprintf(b->url, 2048, "%s", a->url);
            snprintf(b->summary, 1600, "%s", a->body);
            snprintf(b->publisher, 96, "%s", a->publisher);
            snprintf(b->category, 48, "%s", news_category(a->category));
            snprintf(b->published, 96, "%s", a->published);
        }
}
static void press_date_open(int date) {
    press_list_page = 0;
    press_query[0] = 0;
    if (!press_load_day(date))
        press_legacy(date);
    extra_open(95);
    if (date == news_date() && !press_busy()) {
        press_loading_kind = 1;
        press_start_day(date, options[OPT_LANGUAGE]);
    }
}
static void press_read_open(int i) {
    if (i < 0 || i >= press_list.count)
        return;
    if (press_busy())
        press_cancel();
    press_loading_kind = 2;
    int r = press_open(&press_list.item[i]);
    press_reader_hash = 0;
    press_reader_page = 0;
    extra_open(96);
    if (!r)
        ui_message("Artikel konnte nicht geladen werden.");
    else if (!press_busy())
        article_news_open();
}
static void press_image_draw(int y) {
    char p[2048], m[3];
    press_image_path(p, sizeof(p));
    FILE *f = fopen(p, "rb");
    if (!f)
        return;
    int w, h, max;
    if (fscanf(f, "%2s %d %d %d", m, &w, &h, &max) == 4 && !strcmp(m, "P5") && w == 540 &&
        h == 280 && max == 255) {
        fgetc(f);
        for (int row = 0; row < 280 && y + row < 740; row++)
            if (fread(ui_pixels + (y + row) * 600 + 30, 1, 540, f) != 540)
                break;
    }
    fclose(f);
}
static void press_paginate(void) {
    uint64_t key = press_id(press_article.body) ^ press_id(press_article.title);
    if (key == press_reader_hash)
        return;
    press_reader_hash = key;
    press_reader_pages = 0;
    char path[2048];
    press_image_path(path, sizeof(path));
    press_has_image = !access(path, R_OK);
    int title_lines = 0;
    const char *t = press_article.title;
    while (*t && title_lines < 4) {
        t = md_next(t, 540, ui_font(UI_TITLE));
        title_lines++;
    }
    press_initial_y = 119 + title_lines * 36 + 47 + (press_has_image ? 292 : 0);
    if (press_initial_y > 650)
        press_initial_y = 650;
    const char *p = press_article.body;
    while (*p && press_reader_pages < 4096) {
        press_offsets[press_reader_pages] = p - press_article.body;
        int y = press_reader_pages ? 100 : press_initial_y, lines = 0;
        while (*p && y + 30 < 730) {
            if (!press_reader_pages && !lines)
                glyph(&p);
            int inset = !press_reader_pages && lines < 2 ? 48 : 0;
            const char *n = md_next(p, 540 - inset, &paper_text);
            if (n == p)
                break;
            p = n;
            y += 30;
            if (*p == '\n') {
                p++;
                y += 9;
            }
            lines++;
        }
        if (!lines && *p)
            p = md_next(p, 540, &paper_text);
        press_reader_pages++;
    }
    if (!press_reader_pages) {
        press_offsets[0] = 0;
        press_reader_pages = 1;
    }
    if (press_reader_page >= press_reader_pages)
        press_reader_page = press_reader_pages - 1;
}
static void press_reader_draw(void) {
    if (!*press_article.body) {
        icon_button(528, 58, IC_MORE);
        wrapped_raw(30, 140, press_article.title, ui_font(UI_TITLE), 540, 4);
        wrapped(30, 380, "Artikel wird geladen …", &paper_text, 540, 3);
        return;
    }
    press_paginate();
    label_at(30, 59, 472, 32, press_article.publisher, ui_font(UI_SECONDARY));
    icon_button(528, 58, IC_MORE);
    rect(28, 97, 544, 2, 0);
    int y = 100;
    if (!press_reader_page) {
        wrapped_raw(30, 119, press_article.title, ui_font(UI_TITLE), 540, 4);
        int lines = 0;
        const char *t = press_article.title;
        while (*t && lines < 4) {
            t = md_next(t, 540, ui_font(UI_TITLE));
            lines++;
        }
        int sub = 119 + lines * 36 + 8;
        drawtext(30, sub,
                 press_article.complete ? "Aus der Quelle · offline gespeichert"
                                        : "Nur Kurzfassung verfügbar",
                 ui_font(UI_SECONDARY), 540);
        if (press_has_image)
            press_image_draw(sub + 37);
        y = press_initial_y;
    }
    const char *p = press_article.body + press_offsets[press_reader_page];
    int row = 0;
    while (*p && y + 30 < 730) {
        const char *start = p;
        char initial[8] = {0};
        int inset = 0;
        if (!press_reader_page && row == 0) {
            glyph(&p);
            snprintf(initial, sizeof(initial), "%.*s", (int)(p - start), start);
            drawtext_raw(30, y - 8, initial, ui_font(UI_DISPLAY), 60);
        }
        if (!press_reader_page && row < 2)
            inset = 48;
        const char *n = md_next(p, 540 - inset, &paper_text);
        char b[1024];
        snprintf(b, sizeof(b), "%.*s", (int)(n - p), p);
        drawtext_raw(30 + inset, y, b, &paper_text, 540 - inset);
        p = n;
        y += 30;
        if (*p == '\n') {
            p++;
            y += 9;
        }
        row++;
    }
    rect(28, 739, 544, 1, 175);
    pager(746, &press_reader_page, press_reader_pages);
}
static void press_list_draw(void) {
    char date[80];
    if (*press_list.query) {
        snprintf(date, sizeof(date), "%s", press_list.query);
    } else
        snprintf(date, sizeof(date), "%02u.%02u.%04u", press_list.date % 100,
                 press_list.date / 100 % 100, press_list.date / 10000);
    heading_actions("News", 1);
    search_action();
    label_at(30, 120, 540, 28, press_busy() ? tr("Nachrichten werden geladen …") : date, ui_font(UI_SECONDARY));
    for (int r = 0; r < 5 && press_list_page * 5 + r < press_list.count; r++) {
        int i = press_list_page * 5 + r, y = 151 + r * 113;
        PressItem *a = &press_list.item[i];
        char b[180];
        snprintf(b, sizeof(b), "%s · %s", tr(a->category), a->publisher);
        list_row(28, y, 544, 113, -1, 0, a->title, b, ui_font(UI_PRIMARY), 2);
    }
    if (!press_list.count && !press_busy())
        wrapped(30, 180, "Keine Nachrichten verfügbar. Gespeicherte Ausgaben bleiben erhalten.",
                ui_font(UI_TITLE), 540, 5);
    pager(746, &press_list_page, (press_list.count + 4) / 5);
}
static void polish_news_draw(void) {
    if (ui_route() == 96) {
        press_reader_draw();
        return;
    }
    if (ui_route() == 95) {
        press_list_draw();
        return;
    }
    if (ui_route() == 97) {
        heading("Quelle");
        drawtext_raw(30, 145, press_article.publisher, ui_font(UI_PRIMARY), 540);
        wrapped_raw(30, 207, press_article.title, ui_font(UI_TITLE), 540, 3);
        wrapped_raw(30, 370, press_article.url, ui_font(UI_BODY), 540, 7);
        uint8_t temp[qrcodegen_BUFFER_LEN_MAX], qr[qrcodegen_BUFFER_LEN_MAX];
        if (qrcodegen_encodeText(press_article.url, temp, qr, qrcodegen_Ecc_LOW, 1, 12,
                                 qrcodegen_Mask_AUTO, true)) {
            int n = qrcodegen_getSize(qr), scale = 180 / (n + 8);
            for (int yy = 0; yy < n; yy++)
                for (int xx = 0; xx < n; xx++)
                    if (qrcodegen_getModule(qr, xx, yy))
                        rect(210 + (xx + 4) * scale, 550 + (yy + 4) * scale, scale, scale, 0);
        }
        return;
    }
    heading_actions("News", 1);
    search_action();
    label_at(30, 132, 540, 35, tr("Kuratierte Ausgaben"), ui_font(UI_SECONDARY));
    for (int r = 0; r < 7 && press_date_page * 7 + r < press_dates_count; r++) {
        int date = press_dates[press_date_page * 7 + r], y = 181 + r * 75;
        char b[80];
        snprintf(b, sizeof(b), "%02d.%02d.%04d%s", date % 100, date / 100 % 100, date / 10000,
                 date == news_date() ? tr(" · Heute") : "");
        const char *detail =
            tr(date == news_date() ? "Nachrichten des Tages" : "Gespeicherte Ausgabe");
        ListLayout l = list_row(28, y, 544, 75, -1, 62, b, detail, ui_font(UI_PRIMARY), 1);
        icon(528, l.action_y, IC_NEXT);
    }
    pager(746, &press_date_page, (press_dates_count + 6) / 7);
}
static int polish_news_input(const Input *in) {
    if (in->kind == INPUT_SWIPE && ui_route() == 96) {
        if (in->dx < 0 && press_reader_page + 1 < press_reader_pages)
            press_reader_page++;
        else if (in->dx > 0 && press_reader_page)
            press_reader_page--;
        return 1;
    }
    if (in->kind != INPUT_TAP)
        return 2;
    int x = in->x, y = in->y;
    if (ui_route() == 96) {
        if (search_action_hit(x, y))
            extra_open(97);
        else if (y >= 746 && y < 790) {
            if (x < 300 && press_reader_page)
                press_reader_page--;
            else if (x >= 300 && press_reader_page + 1 < press_reader_pages)
                press_reader_page++;
        }
        return 1;
    }
    if (ui_route() == 95) {
        if (search_action_hit(x, y))
            open_keyboard(150, "News suchen", press_query);
        else if (y >= 151 && y < 716) {
            int i = press_list_page * 5 + (y - 151) / 113;
            press_read_open(i);
        } else if (y >= 746 && y < 790) {
            if (x < 300 && press_list_page)
                press_list_page--;
            else if (x >= 300 && (press_list_page + 1) * 5 < press_list.count)
                press_list_page++;
        }
        return 1;
    }
    if (search_action_hit(x, y))
        open_keyboard(150, "News suchen", press_query);
    else if (y >= 181 && y < 706) {
        int i = press_date_page * 7 + (y - 181) / 75;
        if (i < press_dates_count)
            press_date_open(press_dates[i]);
    } else if (y >= 746 && y < 790) {
        if (x < 300 && press_date_page)
            press_date_page--;
        else if (x >= 300 && (press_date_page + 1) * 7 < press_dates_count)
            press_date_page++;
    }
    return 1;
}
static time_t press_retry_at;
static int press_daily_enabled;
