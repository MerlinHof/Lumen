/* Library, reader and their detail views. */
static const char *author_label(const char *author, char *buf, size_t size) {
    const char *comma = strchr(author, ',');
    if (comma && !strchr(comma + 1, ',')) {
        const char *first = comma + 1;
        while (*first == ' ')
            first++;
        snprintf(buf, size, "%s %.*s", first, (int)(comma - author), author);
        return buf;
    }
    return *author ? author : tr("Autor unbekannt");
}
static void library_screen(void) {
    char b[128];
    int last = featured(), per = library_per(), top = library_top(), begin = library_begin();
    if (last >= 0 && listpage == 0) {
        Book *book = &library[last];
        region(28, 80, 544, 232);
        cover_size(book, 28, 80, 152, 208);
        drawtext(206, 96, "W E I T E R L E S E N", &caption, 370);
        wrapped_raw(204, 133, book->title, &editorial, 366, 2);
        {
            char author[256];
            drawtext_raw(206, 235, author_label(book->author, author, sizeof(author)), &small, 366);
        }
        progress(206, 282, 282, book->page + 1, book->total);
        snprintf(b, sizeof(b), tr("%d%%"), book->total ? 100 * (book->page + 1) / book->total : 0);
        drawtext(506, 268, b, &small, 65);
    }
    drawtext(28, last >= 0 && listpage == 0 ? 326 : 76, "M E I N E  B Ü C H E R", &caption, 420);
    for (int r = 0; r < per && begin + r < view_count; r++) {
        Book *book = &library[view[begin + r]];
        int x = 28 + (r % 3) * 188, y = top + (r / 3) * 278;
        region(x, y, 168, 274);
        cover_size(book, x + 8, y, 140, 191);
        wrapped_raw(x + 8, y + 195, book->title, &small, 164, 2);
        {
            char author[256];
            drawtext_raw(x + 8, y + 250, author_label(book->author, author, sizeof(author)),
                         &caption, 164);
        }
    }
    if (!view_count && last < 0)
        wrapped(28, 180, "Deine Bücher erscheinen hier. Im Store kannst du neue Bücher entdecken.",
                &editorial, 544, 3);
    pager(679, &listpage, library_pages());
}
static void reader(void) {
    if (selected < 0)
        return;
    pagination_bind(&page, engine_count(), 0, 0, turn);
    if (!engine_render(page, ui_pixels + 12 * WIDTH + 28, WIDTH, 544, 728, zoom, panx, pany))
        ui_message("Dieses Buch konnte nicht geöffnet oder dargestellt werden.");
    annotations_draw();
    rect(0, 744, 600, 56, 255);
    if (!reader_chrome)
        u15_reader_progress();
    if (reader_chrome) {
        rect(0, 48, 600, 116, 255);
        label_ellipsis(28, 56, 544, 36, library[selected].title, &body, 0);
        label_ellipsis(29, 56, 543, 36, library[selected].title, &body, 0);
        rect(0, 162, 600, 2, 190);

        region(65, 108, 100, 44);
        centered(65, 108, 100, 44, "Aa", &body);
        icon_button(278, 108, IC_CONTENTS);
        icon_button(478, 108, IC_SETTINGS);
    }
}
static const char *reader_families[] = {"Charis", "Sans Serif", "Monospace", "Times", "Noto Serif"};
static const char *reader_align[] = {"Linksbündig", "Blocksatz"};
static const char *reader_zones[] = {"Links / rechts", "Mehr Platz vorwärts"};
static const char *reader_labels[] = {
    "Schriftfamilie", "Schriftgröße",   "Zeilenabstand",        "Seitenrand", "Ausrichtung",
    "Absatzabstand",  "Zeichenabstand", "Wischen zum Blättern", "Tippzonen",  "Lesefortschritt"};
static void settings_screen(void) {
    heading("Lesen");
    drawtext(30, 121, reading_page ? "Abstände & Gesten" : "Schrift & Satz", &caption, 540);
    for (int row = 0; row < 6 && reading_page * 6 + row < 10; row++) {
        int id = reading_page * 6 + row, y = 155 + row * 84;
        form_label(30, y, 310, 44, reader_labels[id]);
        if (id == 0 || id == 4 || id == 8) {
            const char *value = id == 0   ? reader_families[options[OPT_FONT] % 5]
                                : id == 4 ? reader_align[options[OPT_ALIGN] % 2]
                                          : reader_zones[options[OPT_TAP] % 2];
            dropdown_button(350, y, 222, 44, value);
        } else if (id == 7 || id == 9)
            toggle(492, y + 6, options[id == 7 ? OPT_SWIPE : OPT_PROGRESS]);
        else {
            char value[32];
            int v = id == 1   ? settings.size
                    : id == 2 ? settings.leading
                    : id == 3 ? settings.margin
                              : options[id == 5 ? OPT_PARAGRAPH : OPT_LETTERSPACE];
            if (id == 2)
                snprintf(value, sizeof(value), "%.2f", v / 100.0);
            else
                snprintf(value, sizeof(value), "%d", v);
            button_h(394, y, 48, 44, "-", 0);
            centered(444, y, 78, 44, value, &small);
            button_h(524, y, 48, 44, "+", 0);
        }
        divider(28, y + 66, 544);
    }
    pager(679, &reading_page, 2);
}
static void reading_input(const Input *in) {
    if (in->kind != INPUT_TAP)
        return;
    int x = in->x, y = in->y;
    if (y >= 679 && y < 723) {
        reading_page = x >= 300 ? 1 : 0;
        return;
    }
    if (y < 155 || y >= 659)
        return;
    int row = (y - 155) / 84, id = reading_page * 6 + row, top = 155 + row * 84;
    if (id >= 10 || y >= top + 44)
        return;
    if (id == 0 || id == 4 || id == 8) {
        if (x < 350)
            return;
        int opt = id == 0 ? OPT_FONT : id == 4 ? OPT_ALIGN : OPT_TAP;
        const char **labels = id == 0 ? reader_families : id == 4 ? reader_align : reader_zones;
        open_dropdown(100 + opt, labels, id == 0 ? 5 : 2, options[opt], 220, top + 50, 350);
        return;
    }
    if ((id == 7 || id == 9) && x < 484)
        return;
    if (id == 7 || id == 9)
        options[id == 7 ? OPT_SWIPE : OPT_PROGRESS] ^= 1;
    else {
        int direction = x >= 524 ? 1 : x >= 394 && x < 442 ? -1 : 0;
        int *value = id == 1   ? &settings.size
                     : id == 2 ? &settings.leading
                     : id == 3 ? &settings.margin
                               : &options[id == 5 ? OPT_PARAGRAPH : OPT_LETTERSPACE];
        int step = id == 1   ? 2
                   : id == 2 ? 5
                   : id == 3 ? 3
                   : id == 5 ? 4
                             : 2,
            min = id == 1   ? 16
                  : id == 2 ? 110
                            : 0,
            max = id == 1   ? 34
                  : id == 2 ? 190
                  : id == 3 ? 30
                  : id == 5 ? 24
                            : 20;
        *value += step * direction;
        if (*value < min)
            *value = min;
        if (*value > max)
            *value = max;
    }
    library_settings_save();
}
static void chapters_screen(void) {
    heading(chaptertab ? "Lesezeichen" : "Inhalt");
    const char *tabs[] = {"Kapitel", "Lesezeichen"};
    segments(28, 145, 544, 48, tabs, 2, chaptertab);
    int count = chaptertab ? library[selected].bookmark_count : chapter_count;
    for (int r = 0; r < 8 && chapterpage * 8 + r < count; r++) {
        int i = chapterpage * 8 + r;
        char b[280];
        if (chaptertab)
            snprintf(b, sizeof(b), tr("Seite %d"), library[selected].bookmarks[i] + 1);
        else
            snprintf(b, sizeof(b), tr("%s"), chapters[i].title);
        region(28, 213 + r * 57, 544, 56);
        drawtext(30 + (chaptertab ? 0 : chapters[i].depth * 12), 218 + r * 57, b, &body, 530);
        rect(28, 265 + r * 57, 544, 1, 210);
    }
    if (!count)
        drawtext(30, 260,
                 chaptertab ? "Noch keine Lesezeichen." : "Kein Inhaltsverzeichnis vorhanden.",
                 &body, 540);
    button(28, 678, 150, "Lesen", 0);
    if (chapterpage > 0)
        button(205, 678, 165, "Zurück", 0);
    if ((chapterpage + 1) * 8 < count)
        button(398, 678, 174, "Weiter", 0);
}
static void menu(void) {
    heading("Dein Buch");
    wrapped_raw(30, 155, library[selected].title, &body, 540, 2);
    button(30, 270, 540, library[selected].favorite ? "Favorit entfernen" : "Als Favorit markieren",
           0);
    int marked = 0;
    for (int i = 0; i < library[selected].bookmark_count; i++)
        if (library[selected].bookmarks[i] == page)
            marked = 1;
    button(30, 335, 540, marked ? "Lesezeichen entfernen" : "Lesezeichen auf dieser Seite setzen",
           0);
    button(30, 400, 540, "Buchdetails", 0);
    button(30, 465, 540, "Startseite", 0);

    button(30, 530, 540, "Markierungen", 0);
    button(30, 595, 540, "Amazon-Oberfläche öffnen", 0);
    button(30, 665, 540, "Weiterlesen", 0);
}
static const char *keys[] = {"q", "w", "e", "r", "t", "z", "u", "i", "o", "p",
                             "a", "s", "d", "f", "g", "h", "j", "k", "l", "ö",
                             "y", "x", "c", "v", "b", "n", "m", "ä", "ü", "ß"};
static void search_screen(void) {
    heading("Bücher suchen");
    search_field(28, 165, 544, query, "Titel oder Autor eingeben");
    drawtext(30, 245, "Umlaute funktionieren auch als a, o oder u.", &small, 540);
    for (int i = 0; i < 30; i++) {
        int x = 28 + (i % 10) * 55, y = 320 + (i / 10) * 65;
        button(x, y, 49, keys[i], 0);
    }
    button(28, 530, 210, "Leerzeichen", 0);
    button(250, 530, 155, "Löschen", 0);
    button(417, 530, 155, "Leeren", 0);
    button(28, 650, 544, "Ergebnisse anzeigen", 0);
    button(28, 742, 544, "Zurück zur Bibliothek", 0);
}
static void detail(void) {
    if (selected < 0)
        return;
    Book *b = &library[selected];
    heading("Buchdetails");
    cover(b, 30, 152, 1);
    wrapped_raw(176, 150, b->title, &body, 390, 3);
    wrapped_raw(176, 266, *b->author ? b->author : tr("Autor unbekannt"), &small, 390, 2);
    char info[100];
    snprintf(info, sizeof(info), tr("%s  ·  %.1f MB"), b->format, b->size / (1024.0 * 1024));
    drawtext(30, 338, info, &small, 540);
    button(30, 385, 540, library_supported(b) ? "Lesen" : "Im Kindle-Reader lesen", 0);
    button(30, 451, 260, b->favorite ? "Favorit entfernen" : "Favorit", 0);
    button(310, 451, 260, "Umbenennen", 0);
    button(30, 517, 260, "Titel bearbeiten", 0);
    button(310, 517, 260, "Autor bearbeiten", 0);
    button(30, 583, 260, "Buch löschen ...", 0);
    button(310, 583, 260, "Bibliothek", 0);
    if (b->total) {
        snprintf(info, sizeof(info), tr("Seite %d / %d"), b->page + 1, b->total);
        drawtext(30, 688, info, &small, 540);
    }
}
