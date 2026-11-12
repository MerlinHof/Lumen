static void annotations_load(void) {
    reader_ui.annotation_count = reader_ui.annotation_page = 0;
    if (current < 0)
        return;
    char path[2048];
    snprintf(path, sizeof(path), "%s/books/%016llx.notes-v1", data_root,
             (unsigned long long)library[current].id);
    FILE *f = fopen(path, "rb");
    if (!f)
        return;
    reader_ui.annotation_count = (int)fread(reader_ui.annotations, sizeof(Annotation), 128, f);
    fclose(f);
    for (int i = 0; i < reader_ui.annotation_count; i++) {
        reader_ui.annotations[i].text[2047] = 0;
        reader_ui.annotations[i].position.quote[127] = 0;
        if (reader_ui.annotations[i].position.chapter < 0 || reader_ui.annotations[i].position.chapter > 100000)
            reader_ui.annotations[i].position.chapter = 0;
    }
}
static void shade_selection(const TextSelection *s, int shade) {
    for (int i = 0; i < s->count; i++) {
        TextRect r = s->rects[i];
        int x0 = (int)r.x0 + 28, y0 = (int)r.y0 + 12, x1 = (int)ceilf(r.x1) + 28,
            y1 = (int)ceilf(r.y1) + 12;
        if (x0 < 28)
            x0 = 28;
        if (x1 > 572)
            x1 = 572;
        if (y0 < 12)
            y0 = 12;
        if (y1 > 740)
            y1 = 740;
        for (int y = y0; y < y1; y++)
            for (int x = x0; x < x1; x++)
                if (ui_pixels[y * WIDTH + x] > shade)
                    ui_pixels[y * WIDTH + x] = shade;
    }
}
static void annotations_draw(void) {
    if (!reader_ui.annotation_count) {
        if (reader_ui.selecting)
            shade_selection(&reader_ui.selection, 180);
        return;
    }
    if (reader_ui.shading_page != page || reader_ui.shading_notes != reader_ui.annotation_count ||
        reader_ui.shading_epoch != reader_ui.layout_epoch || reader_ui.shading_zoom != zoom || reader_ui.shading_panx != panx ||
        reader_ui.shading_pany != pany) {
        reader_ui.saved_shading.count = 0;
        ReaderPosition pos;
        if (current >= 0 && engine_position(page, &pos)) {
            for (int i = 0; i < reader_ui.annotation_count && reader_ui.saved_shading.count < 64; i++)
                if (reader_ui.annotations[i].position.chapter == pos.chapter) {
                    TextSelection found;
                    if (engine_find_text(page, reader_ui.annotations[i].text, 544, 728, zoom, panx, pany,
                                         &found))
                        for (int j = 0; j < found.count && reader_ui.saved_shading.count < 64; j++)
                            reader_ui.saved_shading.rects[reader_ui.saved_shading.count++] = found.rects[j];
                }
        }
        reader_ui.shading_page = page;
        reader_ui.shading_notes = reader_ui.annotation_count;
        reader_ui.shading_epoch = reader_ui.layout_epoch;
        reader_ui.shading_zoom = zoom;
        reader_ui.shading_panx = panx;
        reader_ui.shading_pany = pany;
    }
    shade_selection(&reader_ui.saved_shading, 215);
    if (reader_ui.selecting)
        shade_selection(&reader_ui.selection, 180);
}
static void selection_overlay(void) {
    const char *actions[] = {"Kopieren", "Wikipedia", "Wörterbuch", "Markieren", "Schließen"};
    selection_menu_draw(READER_SELECTION_Y, reader_ui.selection.text, actions);
}
