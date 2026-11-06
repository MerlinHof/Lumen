/* App-owned module; assembled by apps/host/compose.h. */
static int folder15_index(int id) {
    for (int i = 0; i < note_tree.nf; i++)
        if (note_tree.folder[i].id == id)
            return i;
    return -1;
}
static int folder15_inside(int id, int root) {
    for (int i = 0; i < 513 && id; i++) {
        if (id == root)
            return 1;
        int f = folder15_index(id);
        if (f < 0)
            break;
        id = note_tree.folder[f].parent;
    }
    return 0;
}
static void note15_layout(const char *text) {
    const char *p = text;
    note_rows = 0;
    int line = 1;
    while (*p && note_rows < 8192) {
        note_offsets[note_rows] = p - text;
        note_numbers[note_rows] = line;
        const char *q = note_next_line(
            p, editor_ui.keyboard && editor_ui.keyboard_target != 30 ? 544 : (note_rows % 23 < 2 ? 420 : 528));
        if (q <= p)
            break;
        if (q[-1] == '\n')
            line++;
        p = q;
        note_rows++;
    }
    if (note_rows && p > text && p[-1] == '\n' && note_rows < 8192) {
        note_offsets[note_rows] = p - text;
        note_numbers[note_rows++] = line;
    }
    note_offsets[note_rows] = p - text;
    if (!note_rows) {
        note_numbers[0] = 1;
        note_rows = 1;
        note_offsets[1] = 0;
    }
}
static int u15_note_page_count(void) {
    note15_layout(note_text);
    return (note_rows + 22) / 23;
}
static void u15_note_editor_draw(void) {
    const char *text = editor_ui.keyboard && keyboard_multiline() ? editor_ui.edit : note_text;
    note15_layout(text);
    int editing = editor_ui.keyboard && keyboard_multiline(), rows = editing ? 15 : 23, cursor_row = 0;
    for (int i = 0; i < note_rows; i++)
        if (editor_ui.note_cursor >= note_offsets[i])
            cursor_row = i;
    if (editing && !editor_ui.note_scroll_lock) {
        if (cursor_row < editor_ui.note_view_start)
            editor_ui.note_view_start = cursor_row;
        if (cursor_row >= editor_ui.note_view_start + rows)
            editor_ui.note_view_start = cursor_row - rows + 1;
    }
    if (!editing)
        editor_ui.note_view_start = extra_page * 23;
    int left = editing && editor_ui.keyboard_target != 30 ? 28 : 48;
    for (int r = 0; r < rows; r++)
        rect(left, 78 + r*29, 572-left, 1, 231);
    for (int r = 0; r < rows && editor_ui.note_view_start + r < note_rows; r++) {
        int i = editor_ui.note_view_start + r;
        char b[1024];
        int n = note_offsets[i + 1] - note_offsets[i];
        if (n > 1023)
            n = 1023;
        memcpy(b, text + note_offsets[i], n);
        b[n] = 0;
        char num[20];
        snprintf(num, 20, "%d", note_numbers[i]);
        if (i > 0 && note_numbers[i] == note_numbers[i - 1])
            num[0] = 0;
        ink_color = UI_SECONDARY_INK;
        if (left == 48)
            small_number17(24, 54 + r * 29, num, 135);
        ink_color = 0;
        drawtext_raw(left, 54 + r * 29, b, ui_font(UI_BODY),
                     editing && editor_ui.keyboard_target != 30 ? 544 : (i % 23 < 2 ? 420 : 528));
        if (editing && cursor_row == i && editor_ui.cursor_visible) {
            snprintf(b, sizeof(b), "%.*s", editor_ui.note_cursor - note_offsets[i], text + note_offsets[i]);
            text_caret(left + textwidth_raw(b, ui_font(UI_BODY)), 54 + r * 29, ui_font(UI_BODY));
        }
    }
}
static void u15_note_scroll(int delta) {
    note15_layout(editor_ui.edit);
    int last = note_rows > 15 ? note_rows-15 : 0;
    int first = editor_ui.note_view_start + delta;
    editor_ui.note_view_start = first < 0 ? 0 : first > last ? last : first;
    editor_ui.note_scroll_lock = 1;
}
static void u15_note_editor_touch(int x, int y) {
    note15_layout(editor_ui.edit);
    int row = editor_ui.note_view_start + (y - 54) / 29;
    if (row < 0)
        row = 0;
    if (row >= note_rows)
        row = note_rows - 1;
    const char *p = editor_ui.edit + note_offsets[row], *end = editor_ui.edit + note_offsets[row + 1];
    int px = editor_ui.keyboard && editor_ui.keyboard_target != 30 ? 28 : 48;
    while (p < end && *p != '\n') {
        const char *q = p;
        unsigned c = glyph(&q);
        int w = glyph_advance(ui_font(UI_BODY), c);
        if (x < px + w / 2)
            break;
        px += w;
        p = q;
    }
    editor_ui.note_cursor = p - editor_ui.edit;
}
static void u15_note_tools(void) {
    icon_button(470, 54, IC_EDIT);
    icon_button(532, 54, IC_MARKDOWN);
}
static void u15_export_photo(void);
static void u15_note_overlay(void) {
    ink_load();
    int height = editor_ui.keyboard ? 448 : 688;
    for (int y = 0; y < height; y++)
        for (int x = 0; x < 600; x++) {
            unsigned char v = ink_pixels[y * 600 + x];
            if (v < ui_pixels[(y + 54) * 600 + x])
                ui_pixels[(y + 54) * 600 + x] = v;
        }
    if (note_ink_mode) {
        rect(0, 54, 600, 54, 255);
        icon_button(10, 58, IC_BACK);
        button_h(70, 58, 100, 44, ink_eraser ? "Radierer" : "Stift", ink_eraser);
        button_h(180, 58, 80, 44, ink_width == 3 ? "Fein" : "Breit", 0);
        icon_button(280, 58, IC_UNDO);
        region(518, 58, 64, 44);
        app_icon(528, 58, APP_PHOTOS);
    } else
        u15_note_tools();
}
static void ink_stamp(float x, float y) {
    y -= 54;
    int radius = ink_eraser ? 14 : ink_width;
    float outer = radius + .5f, inner = radius - .5f;
    for (int yy=(int)floorf(y-outer); yy<=y+outer; yy++)
        for (int xx=(int)floorf(x-outer); xx<=x+outer; xx++) {
            if ((unsigned)xx >= 600 || (unsigned)yy >= 688) continue;
            float dx=xx-x, dy=yy-y, distance=dx*dx+dy*dy;
            if (distance >= outer*outer) continue;
            float alpha = distance <= inner*inner ? 1 : outer-sqrtf(distance);
            unsigned char *pixel = &ink_pixels[yy*600+xx];
            int edge = (int)(255*(1-alpha)+.5f);
            if (ink_eraser) {
                if (*pixel < 255-edge) *pixel = 255-edge;
            } else if (*pixel > edge) {
                *pixel = edge;
            }
        }
    ink_dirty = 1;
}
static void ink_line(int x, int y) {
    int dx = x - ink_last_x, dy = y - ink_last_y, n = abs(dx) > abs(dy) ? abs(dx) : abs(dy);
    if (!n)
        n = 1;
    for (int i = 0; i <= n; i++)
        ink_stamp(ink_last_x + dx * (float)i / n, ink_last_y + dy * (float)i / n);
    ink_last_x = x;
    ink_last_y = y;
}
