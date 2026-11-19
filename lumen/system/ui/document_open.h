static int file_open_named(const char *path,int origin,const char *title) {
    const char *ext = strrchr(path, '.');
    int supported = 0;
    const char *types[] = {".epub", ".pdf",  ".txt", ".html", ".htm",
                           ".cbz",  ".mobi", ".azw", ".azw3"};
    for (unsigned i = 0; ext && i < sizeof(types) / sizeof(*types); i++)
        if (!strcasecmp(ext, types[i]))
            supported = 1;
    if (!supported) {
        ui_message("Dieses Format kann der Reader nicht öffnen.");
        return 0;
    }
    ui_save();
    engine_close();
    current = selected = -1;
    Book *b = &library[BOOK_LIMIT];
    memset(b, 0, sizeof(*b));
    b->id = stable_text_id(path);
    snprintf(b->path, sizeof(b->path), "%s", path);
    struct stat document_stat;
    if (!stat(path,&document_stat)) b->size=document_stat.st_size;
    const char *n = strrchr(path, '/');
    snprintf(b->title,sizeof(b->title),"%s",title && *title ? title : n ? n+1 : path);
    library_restore_state(b);
    snprintf(b->format, sizeof(b->format), "%s", ext + 1);
    reader_ui.reader_document_active = 1;
    if (history_count < 64)
        history[history_count++] = origin;
    return ui_open(BOOK_LIMIT);
}
static int file_open16(const char *path,int origin) {
    return file_open_named(path,origin,NULL);
}
static void json_escape16(FILE *f, const char *p) {
    fputc('"', f);
    for (; *p; p++) {
        unsigned char c = *p;
        if (c == '"' || c == '\\') {
            fputc('\\', f);
            fputc(c, f);
        } else if (c < 32)
            fprintf(f, "\\u%04x", c);
        else
            fputc(c, f);
    }
    fputc('"', f);
}
