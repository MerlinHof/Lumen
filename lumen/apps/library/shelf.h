static void update_view(void) {
    library_ui.view_count = library_view(library_ui.view, library_ui.query, 0, settings.sort);
    /* Keep installer fixtures and our welcome note out of the personal shelves. */
    for (int i = 0; i < library_ui.view_count;) {
        const char *name = strrchr(library[library_ui.view[i]].path, '/');
        name = name ? name + 1 : library[library_ui.view[i]].path;
        if (!strcmp(name, "LUMEN Lesetest.epub") || !strcmp(name, "LUMEN – Willkommen.txt")) {
            memmove(library_ui.view + i, library_ui.view + i + 1, (size_t)(library_ui.view_count - i - 1) * sizeof(int));
            library_ui.view_count--;
        } else
            i++;
    }
    int last = featured();
    if (last >= 0)
        for (int i = 0; i < library_ui.view_count; i++)
            if (library_ui.view[i] == last) {
                memmove(library_ui.view + i, library_ui.view + i + 1, (size_t)(library_ui.view_count - i - 1) * sizeof(int));
                library_ui.view_count--;
                break;
            }
    int pinned = 0;
    for (int i = 0; i < library_ui.view_count; i++) {
        if (!library[library_ui.view[i]].favorite)
            continue;
        int index = library_ui.view[i];
        memmove(library_ui.view + pinned + 1, library_ui.view + pinned, (size_t)(i - pinned) * sizeof(*library_ui.view));
        library_ui.view[pinned++] = index;
    }
    int pages = library_pages();
    if (library_ui.listpage >= pages)
        library_ui.listpage = pages - 1;
}
