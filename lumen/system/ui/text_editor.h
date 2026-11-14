/* Shared multiline input viewport. The owning app supplies text through the
 * input contract; no note, journal, todo or LLM-specific layout lives here. */
static FlowLine editor_lines18[8192];
static int editor_count18, editor_first18;
static int editor_numbers18[8192];
static void editor_layout18(void) {
    const char *p = editor_ui.edit;
    editor_count18 = 0;
    int width = 544 - (editor_ui.line_numbers ? 20 : 0), number = 1;
    do {
        const char *start = p;
        int used = 0;
        while (*p && *p != '\n') {
            const char *q = p;
            unsigned c = glyph(&q);
            int a = glyph_advance(&small, c);
            if (used + a > width && p > start)
                break;
            used += a;
            p = q;
        }
        editor_numbers18[editor_count18] = number;
        editor_lines18[editor_count18++] = (FlowLine){start - editor_ui.edit, p - editor_ui.edit};
        if (*p == '\n') {
            number++;
            p++;
            if (!*p) {
                editor_numbers18[editor_count18] = number;
                editor_lines18[editor_count18++] = (FlowLine){p - editor_ui.edit, p - editor_ui.edit};
                break;
            }
        }
    } while (*p && editor_count18 < 8191);
    int row = 0;
    for (int i = 0; i < editor_count18; i++)
        if (editor_ui.note_cursor >= editor_lines18[i].start)
            row = i;
    if (!editor_ui.note_scroll_lock) {
        if (row < editor_first18)
            editor_first18 = row;
        if (row >= editor_first18 + 14)
            editor_first18 = row - 13;
    }
    if (editor_first18 >= editor_count18)
        editor_first18 = editor_count18 - 1;
    if (editor_first18 < 0)
        editor_first18 = 0;
}
static void editor_draw18(void) {
    editor_layout18();
    int left = 28 + (editor_ui.line_numbers ? 20 : 0);
    for (int r = 0; r < 14 && editor_first18 + r < editor_count18; r++) {
        int i = editor_first18 + r;
        FlowLine line = editor_lines18[i];
        char b[8192];
        snprintf(b, sizeof(b), "%.*s", line.end - line.start, editor_ui.edit + line.start);
        if ((editor_ui.line_numbers ? 20 : 0) &&
            (i == 0 || editor_numbers18[i] != editor_numbers18[i - 1])) {
            char num[16];
            snprintf(num, sizeof(num), "%d", editor_numbers18[i]);
            small_number17(left / 2, 54 + r * 29, num, 135);
        }
        drawtext_raw(left, 54 + r * 29, b, &small, 544 - (editor_ui.line_numbers ? 20 : 0));
        int active = editor_ui.note_cursor >= line.start && editor_ui.note_cursor <= line.end;
        if (i + 1 < editor_count18 && editor_ui.note_cursor == editor_lines18[i + 1].start)
            active = 0;
        if (active && editor_ui.cursor_visible) {
            snprintf(b, sizeof(b), "%.*s", editor_ui.note_cursor - line.start, editor_ui.edit + line.start);
            text_caret(left + textwidth_raw(b, &small), 54 + r * 29, &small);
        }
    }
}
static void editor_place18(int x, int y) {
    editor_layout18();
    int row = editor_first18 + (y - 54) / 29;
    if (row < 0)
        row = 0;
    if (row >= editor_count18)
        row = editor_count18 - 1;
    FlowLine line = editor_lines18[row];
    const char *p = editor_ui.edit + line.start, *end = editor_ui.edit + line.end;
    int xx = 28 + (editor_ui.line_numbers ? 20 : 0);
    while (p < end) {
        const char *q = p;
        unsigned c = glyph(&q);
        int a = glyph_advance(&small, c);
        if (x < xx + a / 2)
            break;
        xx += a;
        p = q;
    }
    editor_ui.note_cursor = p - editor_ui.edit;
    caret_restart();
}

static void editor_reset18(void) {
    editor_first18 = 0;
}
static void editor_scroll18(int delta) {
    editor_layout18();
    editor_first18 += delta;
    if (editor_first18 < 0)
        editor_first18 = 0;
    if (editor_first18 > editor_count18 - 14)
        editor_first18 = editor_count18 > 14 ? editor_count18 - 14 : 0;
    editor_ui.note_scroll_lock = 1;
}
