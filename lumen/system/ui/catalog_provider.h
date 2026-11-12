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
    store_ui.opds_depth = 0;
    opds_count = 0;
    if (!store_ui.store_provider_count) {
        store_ui.store_stage = 0;
        ui_message("Quellen in den Store-Einstellungen aktivieren.");
        return;
    }
    if (!discover_start(store_ui.store_urls[0], NULL))
        store_complete(-1);
}
static void store_seed(void) {
    if(opds_count || discover_busy())return;
    char path[2048];
    snprintf(path,sizeof(path),"%s/../catalogs/gutenberg-de.opds",data_root);
    discover_parse(path,store_ui.opds_source);
}
static int store_item(const char *key) {
    if(strlen(key)!=16 || strspn(key,"0123456789abcdef")!=16)return -1;
    uint64_t id=strtoull(key,NULL,16);
    for(int i=0;i<opds_count;i++)if(press_id(opds_items[i].url)==id)return i;
    return -1;
}
