static int text_editable_hit(const Input *in) {
    if (editor_ui.keyboard)
        return keyboard_multiline() ? (in->y >= 54 && in->y < 458) : (in->y >= 452 && in->y < 501);
    if (ui_route() == 27)
        return in->x >= 28 && in->x < 514 && in->y >= 361 && in->y < 413;
    return ui_route() == 30 && !note_ink_mode && in->y >= 54 && in->y < 742;
}
/* Shared editable-text clipboard and word selection. UI buttons never become selection targets. */
static int clip_s, clip_e;
static char clipboard_text[8192], clip_source[8192];
static void selection_span(int left, int top, const Font *font, const char *text, int start,
                           int end, int first, int last, int width) {
    if (last <= start || first >= end)
        return;
    int a = first > start ? first : start, b = last < end ? last : end;
    char prefix[8192];
    snprintf(prefix, sizeof(prefix), "%.*s", a - start, text + start);
    int x = left + textwidth_raw(prefix, font);
    snprintf(prefix, sizeof(prefix), "%.*s", b - start, text + start);
    int right = left + textwidth_raw(prefix, font);
    if (right > left + width)
        right = left + width;
    for (int y = top; y < top + font->h - 4 && y < HEIGHT; y++)
        for (int xx = x; xx < right && xx < WIDTH; xx++)
            if (y >= 0 && xx >= 0)
                if (ui_pixels[y * WIDTH + xx] > 180) ui_pixels[y * WIDTH + xx] = 180;
}
static void clipboard_draw(void) {
    if (!editor_ui.clip_active)
        return;
    if (editor_ui.keyboard && keyboard_multiline()) {
        for (int r = 0; r < 14 && editor_first18 + r < editor_count18; r++) {
            FlowLine line = editor_lines18[editor_first18 + r];
            selection_span(28 + text_input_spec(editor_ui.keyboard_target).gutter, 54 + r * 29, &small, editor_ui.edit,
                           line.start, line.end, clip_s, clip_e,
                           544 - text_input_spec(editor_ui.keyboard_target).gutter);
        }
    } else if (editor_ui.keyboard)
        selection_span(28, 459, &body, editor_ui.edit, editor_ui.keyboard_view18, strlen(editor_ui.edit), clip_s, clip_e, 535);
    else if (ui_route() == 27)
        selection_span(43, 372, &small, calc_expr, calc_input_start(), strlen(calc_expr), clip_s,
                       clip_e, 468);
    if (editor_ui.clip_active == 1)
        return;
    char b[1024];
    int n = clip_e - clip_s;
    if (n > 1023)
        n = 1023;
    if (n < 0)
        n = 0;
    memcpy(b, clip_source + clip_s, n);
    b[n] = 0;
    const char *actions[] = {"Kopieren", "Alles", "Schließen", "Ausschneiden", "Einfügen"};
    selection_menu_draw(EDIT_SELECTION_Y, b, actions);
}
static char *clipboard_edit_buffer18(void) {
    return editor_ui.keyboard ? editor_ui.edit : ui_route() == 27 ? calc_expr : NULL;
}
static int *clipboard_edit_cursor18(void) {
    return editor_ui.keyboard ? &editor_ui.note_cursor : &calc_cursor;
}
static void clipboard_paste(void) {
    if (!clipboard_edit_buffer18() || !*clipboard_text) {
        ui_message("Zum Einfügen zuerst ein Textfeld öffnen.");
        return;
    }
    if (!text_insert(clipboard_edit_buffer18(), editor_ui.keyboard ? keyboard_capacity() : sizeof(calc_expr),
                     clipboard_edit_cursor18(), clipboard_text))
        ui_message("Text passt nicht in dieses Feld.");
}
static int clip_anchor, clip_dragged;
static int clipboard_position(int x, int y) {
    if (editor_ui.keyboard) {
        if (keyboard_multiline())
            editor_place18(x, y);
        else
            keyboard_place18(x);
        return editor_ui.note_cursor;
    }
    const char *p = calc_expr + calc_input_start();
    int xx = 43;
    while (*p) {
        const char *q = p;
        int a = glyph_advance(&small, glyph(&q));
        if (x < xx + a / 2)
            break;
        xx += a;
        p = q;
    }
    calc_cursor = p - calc_expr;
    return calc_cursor;
}
static int clipboard_input(const Input *in) {
    if (ui_route() == 2 && reader_ui.selecting == 2 && in->kind == INPUT_TAP && selection_menu_action(READER_SELECTION_Y, in->x, in->y) == 0) {
        snprintf(clipboard_text, sizeof(clipboard_text), "%s", reader_ui.selection.text);
        reader_ui.selecting = 0;
        ui_message("Kopiert.");
        return 1;
    }
    if (editor_ui.clip_active == 1) {
        if (in->kind == INPUT_MOVE || in->kind == INPUT_TAP || in->kind == INPUT_SWIPE) {
            int end = clipboard_position(in->x, in->y);
            clip_s = end < clip_anchor ? end : clip_anchor;
            clip_e = end > clip_anchor ? end : clip_anchor;
            clip_dragged |= end != clip_anchor;
            if (in->kind != INPUT_MOVE) {
                if (!clip_dragged) {
                    while (clip_s > 0 && clip_source[clip_s - 1] != ' ' &&
                           clip_source[clip_s - 1] != '\n')
                        clip_s--;
                    while (clip_source[clip_e] && clip_source[clip_e] != ' ' &&
                           clip_source[clip_e] != '\n')
                        clip_e++;
                    clip_s = text_boundary(clip_source, clip_s);
                }
                editor_ui.clip_active = 2;
                pressed = -1;
            }
            return 1;
        }
        return 1;
    }
    if (editor_ui.clip_active == 2) {
        if (in->kind == INPUT_TAP) {
            int action = selection_menu_action(EDIT_SELECTION_Y, in->x, in->y);
            char *buffer = clipboard_edit_buffer18();
            int *cursor = clipboard_edit_cursor18();
            if (action >= 0 && action < 3) {
                if (action == 0) {
                    int n = clip_e - clip_s;
                    memcpy(clipboard_text, clip_source + clip_s, n);
                    clipboard_text[n] = 0;
                    editor_ui.clip_active = 0;
                    ui_message("Kopiert.");
                } else if (action == 1) {
                    clip_s = 0;
                    clip_e = strlen(clip_source);
                } else
                    editor_ui.clip_active = 0;
            } else if (action >= 3) {
                if (buffer && action == 3) {
                    int n = clip_e - clip_s;
                    memcpy(clipboard_text, clip_source + clip_s, n);
                    clipboard_text[n] = 0;
                    text_erase(buffer, cursor, clip_s, clip_e);
                } else if (buffer) {
                    size_t capacity = editor_ui.keyboard ? keyboard_capacity() : sizeof(calc_expr);
                    if (strlen(buffer) - (clip_e - clip_s) + strlen(clipboard_text) < capacity) {
                        text_erase(buffer, cursor, clip_s, clip_e);
                        clipboard_paste();
                    }
                }
                if (!editor_ui.keyboard && ui_route() == 27)
                    calc_preview();
                editor_ui.clip_active = 0;
            } else
                editor_ui.clip_active = 0;
        }
        return 1;
    }
    if (in->kind != INPUT_HOLD || ui_route() == 2 || !text_editable_hit(in))
        return 0;
    if (!editor_ui.keyboard && ui_route() == 30) {
        open_keyboard(30, "Notiz", note_text);
    }
    if (editor_ui.keyboard && (editor_ui.keyboard_target == 5 || editor_ui.keyboard_target == 32 || editor_ui.keyboard_target == 176))
        return 0;
    char *buffer = clipboard_edit_buffer18();
    if (!buffer)
        return 0;
    clip_anchor = clipboard_position(in->x, in->y);
    clip_s = clip_e = clip_anchor;
    clip_dragged = 0;
    snprintf(clip_source, sizeof(clip_source), "%s", buffer);
    editor_ui.clip_active = 1;
    pressed = -1;
    return 1;
}
