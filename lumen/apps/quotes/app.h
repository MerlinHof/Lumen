/* App-owned module; assembled by apps/host/compose.h. */
static void quotes_load(void) {
    global_quote_count = 0;
    memset(quote_counts, 0, sizeof(quote_counts));
    for (int b = 0; b < library_count; b++) {
        char path[2048];
        snprintf(path, sizeof(path), "%s/books/%016llx.notes-v1", data_root,
                 (unsigned long long)library[b].id);
        struct stat st;
        if (!stat(path, &st) && st.st_size > 0) {
            int count = (int)(st.st_size / sizeof(Annotation));
            if (count > 128)
                count = 128;
            quote_counts[b] = count;
            global_quote_count += count;
        }
    }
    if (global_quote_page * 4 >= global_quote_count)
        global_quote_page = 0;
}
static int quote_get(int index, GlobalQuote *q) {
    if (index < 0)
        return 0;
    for (int b = 0; b < library_count; b++) {
        if (index >= quote_counts[b]) {
            index -= quote_counts[b];
            continue;
        }
        char path[2048];
        snprintf(path, sizeof(path), "%s/books/%016llx.notes-v1", data_root,
                 (unsigned long long)library[b].id);
        FILE *f = fopen(path, "rb");
        if (!f)
            return 0;
        Annotation a;
        int ok = !fseek(f, (long)index * sizeof(a), SEEK_SET) && fread(&a, sizeof(a), 1, f) == 1;
        fclose(f);
        if (!ok)
            return 0;
        a.text[sizeof(a.text) - 1] = 0;
        q->book = b;
        q->index = index;
        q->position = a.position;
        snprintf(q->text, sizeof(q->text), "%s", a.text);
        return 1;
    }
    return 0;
}
static int quote_hit_top[4], quote_hit_bottom[4], quote_hit_count;
static void app_quotes(void) {
    heading("Zitate");
    quote_hit_count = 0;
    int y = 146;
    for (int r = 0; r < 4 && global_quote_page * 4 + r < global_quote_count; r++) {
        GlobalQuote row, *q = &row;
        if (!quote_get(global_quote_page * 4 + r, q))
            continue;

        const char *p = q->text;
        int lines = 0;
        while (*p && lines < 3) {
            p = note_next_line(p, 532);
            lines++;
        }
        wrapped_raw(34, y, q->text, ui_font(UI_BODY), 532, 3);
        drawtext_raw(34, y + lines * (small.h - 4) + 5, library[q->book].title, ui_font(UI_SECONDARY), 532);
        int bottom = y + lines * (small.h - 4) + 34;
        region(28, y, 544, bottom - y);
        quote_hit_top[quote_hit_count] = y;
        quote_hit_bottom[quote_hit_count++] = bottom;
        divider(28, bottom, 544);
        y = bottom + 16;
    }
    if (!global_quote_count)
        wrapped(30, 180, "Alle markierten Zitate deiner Bücher erscheinen hier.", ui_font(UI_TITLE), 540,
                4);
    pager(745, &global_quote_page, (global_quote_count + 3) / 4);
}
