/* App-owned module; assembled by apps/host/compose.h. */
static void open_store(void) {
    route_to(10);
    store_ui.store_home = !*store_ui.store_query;
    if (!opds_count) {
        char path[2048];
        snprintf(path, sizeof(path), tr("%s/../catalogs/gutenberg-de.opds"), data_root);
        discover_parse(path, store_ui.opds_source);
    }
#ifndef LUMEN_HOST
    if (!discover_busy())
        store_search();
#else
    if (getenv("LUMEN_STORE_NETWORK") && !discover_busy())
        store_search();
#endif
}
