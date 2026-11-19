typedef struct {
    int store_stage;
    int store_total;
    int store_federated;
    OPDSItem store_results[OPDS_LIMIT];
    char store_urls[12][2048];
    int store_provider_count;
    char store_query[256];
    int opds_depth;
    char opds_history[16][2048];
    char opds_source[2048];
} StoreUi;
static StoreUi store_ui = {
    .store_provider_count = 2,
    .opds_source = "https://www.gutenberg.org/ebooks/search.opds/?query=l.de&sort_order=downloads",
};
#include "sources.h"
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
        store_notice("Keine Treffer. Suche oder Verbindung prüfen.","No results. Check the search or connection.","Aucun résultat. Vérifiez la recherche ou la connexion.");
    return 1;
}
static void store_search(void) {
    char encoded[1200];
    const char *term=*store_ui.store_query ? store_ui.store_query : "books";
    if(!lumen_url_encode(term,encoded,sizeof(encoded)))return;
    snprintf(store_ui.store_urls[0], sizeof(store_ui.store_urls[0]),
             "https://www.gutenberg.org/ebooks/search.opds/?query=%s&sort_order=downloads",
             *store_ui.store_query                 ? encoded
             : lumen_language == 0 ? "l.de"
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
        store_notice("Quellen in den Store-Einstellungen aktivieren.","Enable a catalog in Store settings.","Activez un catalogue dans les réglages.");
        return;
    }
    if (!discover_start(store_ui.store_urls[0], NULL))
        store_complete(-1);
}
static void store_seed(void) {
    if(opds_count || discover_busy())return;
    const char *root=getenv("LUMEN_APP_ASSETS");
    char path[4096];
    if(root && snprintf(path,sizeof(path),"%s/assets/gutenberg-de.opds",root)<(int)sizeof(path))
        discover_parse(path,store_ui.opds_source);
}
