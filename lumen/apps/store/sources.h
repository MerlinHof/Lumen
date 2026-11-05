/* App-owned module; assembled by apps/host/compose.h. */
static void sources15_save(void) {
    sources15_dirty = 1;
    char p[2048];
    local_path(p, sizeof(p), "store-sources15.bin");
    atomic_write(p, source15, sizeof(source15));
}
static void u15_init(void) {
    if (u15_ready)
        return;
    u15_ready = 1;
    const char *n[] = {"Project Gutenberg", "Internet Archive", "epubBooks", "BDeBooks",
                       "Standard Ebooks"};
    const char *u[] = {"", "", "https://www.epubbooks.com/search?q={q}",
                       "https://bdebooks.com/en/all-books/?q={q}",
                       "https://standardebooks.org/feeds/opds"};
    for (int i = 0; i < 5; i++) {
        snprintf(source15[i].name, 80, "%s", n[i]);
        snprintf(source15[i].url, 2048, "%s", u[i]);
        source15[i].enabled = 1;
    }
    source15_count = 5;
    char p[2048];
    local_path(p, sizeof(p), "store-sources15.bin");
    FILE *f = fopen(p, "rb");
    if (f) {
        if (fread(source15, sizeof(source15), 1, f) != 1)
            memset(source15, 0, sizeof(source15));
        fclose(f);
        for (int i = 0; i < 10; i++) {
            source15[i].name[79] = source15[i].url[2047] = 0;
        }
        source15_count = 0;
        while (source15_count < 10 && *source15[source15_count].name)
            source15_count++;
    }
    local_path(p, sizeof(p), "reader-progress15.txt");
    f = fopen(p, "r");
    if (f) {
        fscanf(f, "%d", &reader_ui.reader_progress_mode);
        fclose(f);
    }
    if (reader_ui.reader_progress_mode < 0 || reader_ui.reader_progress_mode > 6)
        reader_ui.reader_progress_mode = 0;
}
static void u15_sources_prepare(const char *encoded) {
    u15_init();
    char first[2][2048];
    memcpy(first, store_ui.store_urls, sizeof(first));
    store_ui.store_provider_count = 0;
    for (int i = 0; i < source15_count && store_ui.store_provider_count < 12; i++)
        if (source15[i].enabled) {
            char *dest = store_ui.store_urls[store_ui.store_provider_count];
            if (i < 2)
                snprintf(dest, 2048, "%s", first[i]);
            else {
                const char *q = strstr(source15[i].url, "{q}");
                if (q)
                    snprintf(dest, 2048, "%.*s%s%s", (int)(q - source15[i].url), source15[i].url,
                             encoded, q + 3);
                else
                    snprintf(dest, 2048, "%s", source15[i].url);
            }
            store_ui.store_provider_count++;
        }
}
