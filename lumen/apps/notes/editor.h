/* App-owned module; assembled by apps/host/compose.h. */
static void app_notes(void) {
    v1_notes_draw();
}
static const char *note_next_line(const char *p, int width) {
    const char *q = p, *space = NULL;
    int w = 0;
    while (*q && *q != '\n') {
        const char *old = q;
        unsigned c = glyph(&q);
        if (w + glyph_advance(ui_font(UI_BODY), c) > width) {
            q = space && space > p ? space : old;
            break;
        }
        w += glyph_advance(ui_font(UI_BODY), c);
        if (c == ' ')
            space = q;
    }
    if (q == p && *q && *q != '\n')
        glyph(&q);
    if (*q == '\n')
        q++;
    return q;
}
static void note_editor_draw(void) {
    u15_note_editor_draw();
}
static void note_editor_touch(int x, int y) {
    u15_note_editor_touch(x, y);
}
static void app_note(void) {
    if (!editor_ui.keyboard)
        v1_note_tools();
    note_editor_draw();
    if (!editor_ui.keyboard)
        pager(748, &extra_page, u15_note_page_count());
    u15_note_overlay();
}
