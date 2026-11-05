typedef struct {
    int view[BOOK_LIMIT], view_count, listpage, library_dirty;
    char query[128];
} LibraryUi;
static LibraryUi library_ui;
