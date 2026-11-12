/* State owned by the store app in the legacy view host. */
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
