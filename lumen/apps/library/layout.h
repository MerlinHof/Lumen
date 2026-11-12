static int featured(void) {
    return library_last();
}
static int library_per(void) {
    return featured() >= 0 && library_ui.listpage == 0 ? 3 : 6;
}
static int library_begin(void) {
    return featured() >= 0 ? (library_ui.listpage ? 3 + (library_ui.listpage - 1) * 6 : 0) : library_ui.listpage * 6;
}
static int library_pages(void) {
    return featured() >= 0 ? 1 + (library_ui.view_count > 3 ? (library_ui.view_count - 3 + 5) / 6 : 0)
                           : (library_ui.view_count ? (library_ui.view_count + 5) / 6 : 1);
}
