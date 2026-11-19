static int u15_ready;
static struct {
    char name[80], url[2048];
    int enabled;
} source15[10];
static int source15_count;
/* App-owned module; assembled by apps/host/compose.h. */
static void sources15_save(void) {
    lumen_storage_write("sources.bin",source15,sizeof(source15));
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
    typeof(source15) imported;
    size_t size;
    const char *legacy=getenv("LUMEN_LEGACY_DATA");
    if((lumen_storage_read("sources.bin",imported,sizeof(imported),&size) ||
        (legacy && lumen_storage_read_at(legacy,"store-sources15.bin",imported,sizeof(imported),&size))) &&
       size==sizeof(imported)) {
        memcpy(source15,imported,sizeof(source15));
        for (int i = 0; i < 10; i++) {
            source15[i].name[79] = source15[i].url[2047] = 0;
        }
        source15_count = 0;
        while (source15_count < 10 && *source15[source15_count].name)
            source15_count++;
    }
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
