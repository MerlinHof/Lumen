/* App-owned module; assembled by apps/host/compose.h. */
static const char *news_category(int c) {
    const char *labels[] = {"Welt",    "Wissenschaft",  "Technologie",
                            "Energie", "An diesem Tag", "Tagesrätsel"};
    return tr(labels[c >= 0 && c < 6 ? c : 0]);
}
