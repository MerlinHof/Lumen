/* App-owned module; assembled by apps/host/compose.h. */
static int wiki_image_available(void) {
    if (!*discover_lookup_image())
        return 0;
    char *key = service_read("wiki-image-url.txt", 2047);
    int same = key && !strcmp(key, discover_lookup_image());
    free(key);
    char p[2048];
    struct stat st;
    local_path(p, sizeof(p), "wiki-image.pgm");
    return same && !stat(p, &st) && st.st_size >= 130000;
}
static void app_wiki(void) {
    wiki_history_current();
    polish_wiki_history();
    if (discover_busy() || wiki_reader_pending) {
        rect(80, 235, 440, 180, 255);
        key_outline(80, 235, 440, 180);
        aa_arc(300, 285, 17, 35, 310, 4);
        centered(95, 325, 410, 60, "Artikel wird geladen …", ui_font(UI_PRIMARY));
    }
}

typedef struct {
    int book, index;
    char text[2048];
    ReaderPosition position;
} GlobalQuote;
static int quote_counts[BOOK_LIMIT], global_quote_count, global_quote_page;
