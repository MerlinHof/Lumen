/* App-owned module; assembled by apps/host/compose.h. */
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
        drawtext(30 + (chaptertab ? 0 : chapters[i].depth * 12), 218 + r * 57, b, ui_font(UI_PRIMARY), 530);
        rect(28, 265 + r * 57, 544, 1, 210);
    }
    if (!count)
        drawtext(30, 260,
                 chaptertab ? "Noch keine Lesezeichen." : "Kein Inhaltsverzeichnis vorhanden.",
                 ui_font(UI_PRIMARY), 540);
    button(28, 678, 150, "Lesen", 0);
    if (chapterpage > 0)
        button(205, 678, 165, "Zurück", 0);
    if ((chapterpage + 1) * 8 < count)
        button(398, 678, 174, "Weiter", 0);
}
static void menu(void) {
    heading("Dein Buch");
    wrapped_raw(30, 155, library[selected].title, ui_font(UI_PRIMARY), 540, 2);
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
