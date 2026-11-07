/* App-owned module; assembled by apps/host/compose.h. */
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
    if (last >= 0 && library_ui.listpage == 0) {
        Book *book = &library[last];
        region(28, 80, 544, 232);
        cover_size(book, 28, 80, 152, 208);
        drawtext(206, 96, "W E I T E R L E S E N", ui_font(UI_SECONDARY), 315);
        wrapped_raw(204, 133, book->title, ui_font(UI_TITLE), 366, 2);
        {
            char author[256];
            drawtext_raw(206, 235, author_label(book->author, author, sizeof(author)), ui_font(UI_BODY), 366);
        }
        progress(206, 282, 282, book->page + 1, book->total);
        snprintf(b, sizeof(b), tr("%d%%"), book->total ? 100 * (book->page + 1) / book->total : 0);
        drawtext(506, 268, b, ui_font(UI_BODY), 65);
    }
    drawtext(28, last >= 0 && library_ui.listpage == 0 ? 326 : 76, "M E I N E  B Ü C H E R", ui_font(UI_SECONDARY), 420);
    for (int r = 0; r < per && begin + r < library_ui.view_count; r++) {
        Book *book = &library[library_ui.view[begin + r]];
        int x = 28 + (r % 3) * 188, y = top + (r / 3) * library_row_height();
        region(x, y, 168, library_row_height()-4);
        cover_size(book, x, y, 164, 224);
        if (book->favorite) {
            control_surface(x + 109, y + 7, 32, 32, 7, 255);
            icon_sized(x + 111, y + 9, IC_PIN, 28);
        }
        wrapped_raw(x + 8, y + 229, book->title, ui_font(UI_BODY), 164, 2);
        {
            char author[256];
            drawtext_raw(x + 8, y + 278, author_label(book->author, author, sizeof(author)),
                         ui_font(UI_SECONDARY), 164);
        }
    }
    if (!library_ui.view_count && last < 0)
        wrapped(28, 180, "Deine Bücher erscheinen hier. Im Store kannst du neue Bücher entdecken.",
                ui_font(UI_TITLE), 544, 3);
    icon_button(528,60,IC_SETTINGS);
    pager(745, &library_ui.listpage, library_pages());
}
