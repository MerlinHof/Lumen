/* State owned by the wikipedia app in the legacy view host. */
typedef struct {
    int wiki_article_language;
} WikiUi;
static WikiUi wiki_ui = {
    .wiki_article_language = -1,
};
