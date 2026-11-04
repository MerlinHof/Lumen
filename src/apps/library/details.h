/* App-owned module; assembled by apps/host/compose.h. */
static void search_screen(void) {
    heading("Bücher suchen");
    search_field(28, 165, 544, query, "Titel oder Autor eingeben");
    drawtext(30, 245, "Umlaute funktionieren auch als a, o oder u.", ui_font(UI_BODY), 540);
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
    wrapped_raw(176, 150, b->title, ui_font(UI_PRIMARY), 390, 3);
    wrapped_raw(176, 266, *b->author ? b->author : tr("Autor unbekannt"), ui_font(UI_BODY), 390, 2);
    char info[100];
    snprintf(info, sizeof(info), tr("%s  ·  %.1f MB"), b->format, b->size / (1024.0 * 1024));
    drawtext(30, 338, info, ui_font(UI_BODY), 540);
    button(30, 385, 540, library_supported(b) ? "Lesen" : "Im Kindle-Reader lesen", 0);
    button(30, 451, 260, b->favorite ? "Favorit entfernen" : "Favorit", 0);
    button(310, 451, 260, "Umbenennen", 0);
    button(30, 517, 260, "Titel bearbeiten", 0);
    button(310, 517, 260, "Autor bearbeiten", 0);
    button(30, 583, 260, "Buch löschen ...", 0);
    button(310, 583, 260, "Bibliothek", 0);
    if (b->total) {
        snprintf(info, sizeof(info), tr("Seite %d / %d"), b->page + 1, b->total);
        drawtext(30, 688, info, ui_font(UI_BODY), 540);
    }
}
