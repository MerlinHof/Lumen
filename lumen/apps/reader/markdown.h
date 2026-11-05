/* App-owned module; assembled by apps/host/compose.h. */
static void reader_markdown17(const char *title, const char *text, int origin) {
    char dir[2048], path[4096];
    local_path(dir, sizeof(dir), "reader-documents");
    mkdir(dir, 0700);
    snprintf(path, sizeof(path), "%s/md-%016llx.html", dir,
             (unsigned long long)(press_id(title) ^ press_id(text)));
    FILE *f = fopen(path, "w");
    if (!f)
        return;
    fputs("<!doctype html><meta charset=utf-8><style>body{font-family:serif}p{margin:0 0 "
          ".7em}h1,h2,h3{line-height:1.15;margin:1em 0 "
          ".5em}pre{white-space:pre-wrap;font-size:.85em}blockquote{border-left:2px solid "
          "#888;padding-left:1em}li{margin-bottom:.3em}code{font-family:monospace}</style>",
          f);
    markdown_render17(f, text);
    fclose(f);
    if (file_open16(path, origin)) {
        snprintf(library[BOOK_LIMIT].title, sizeof(library[BOOK_LIMIT].title), "%s", title);
        snprintf(library[BOOK_LIMIT].author, sizeof(library[BOOK_LIMIT].author), "LUMEN");
    }
}
