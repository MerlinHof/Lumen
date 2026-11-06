static int keyboard_multiline(void) {
    return text_input_spec(editor_ui.keyboard_target).multiline;
}
static void keyboard_insert(const char *text) {
    editor_ui.note_scroll_lock = 0;
    text_insert(editor_ui.edit, keyboard_capacity(), &editor_ui.note_cursor, text);
    editor_ui.cursor_visible = 1;
    editor_ui.cursor_epoch = extra_millis();
}
static void keyboard_backspace(void) {
    editor_ui.note_scroll_lock = 0;
    text_backspace(editor_ui.edit, &editor_ui.note_cursor);
    editor_ui.cursor_visible = 1;
    editor_ui.cursor_epoch = extra_millis();
}

/* System keyboard: one geometry for drawing, touch feedback and text dispatch.
 * Independent layout preference; all applications share UTF-8 caret editing. */
typedef struct {
    int x, y, w, h, action;
    char text[8];
} KeyboardKey18;
static KeyboardKey18 keyboard_keys18[56];
static int keyboard_count18;
static int keyboard_frame18 = -1;
static unsigned char keyboard_keybed18[600 * 298];
enum {
    KEY_TEXT18,
    KEY_SHIFT18,
    KEY_DELETE18,
    KEY_SYMBOL18,
    KEY_LANGUAGE18,
    KEY_SPACE18,
    KEY_ENTER18
};
static void keyboard_case18(char *key) {
    if (!editor_ui.keyboard_shift || editor_ui.keyboard_symbols)
        return;
    const char *p = key;
    unsigned c = utf8_next(&p);
    if (c >= 'a' && c <= 'z')
        key[0] -= 32;
    else if (c >= 0xe0 && c <= 0xfe && c != 0xf7) {
        c -= 32;
        key[0] = (char)(0xc0 | (c >> 6));
        key[1] = (char)(0x80 | (c & 63));
        key[2] = 0;
    } else if (c == 0x153)
        snprintf(key, 8, "Œ");
    else if (c == 0xdf)
        snprintf(key, 8, "ẞ");
}
static void keyboard_add18(int x, int y, int w, int h, int action, const char *text) {
    KeyboardKey18 *k = &keyboard_keys18[keyboard_count18++];
    *k = (KeyboardKey18){.x = x, .y = y, .w = w, .h = h, .action = action};
    snprintf(k->text, sizeof(k->text), "%s", text);
    if (action == KEY_TEXT18)
        keyboard_case18(k->text);
}
static void keyboard_layout18(void) {
    static int previous_layout = -1;
    int language = preferences_keyboard_language();
    int layout = language*16 + editor_ui.keyboard_symbols*2 + editor_ui.keyboard_shift;
    if (layout == previous_layout) return;
    previous_layout = layout;
    keyboard_count18 = 0;
    for (int i = 0; i < 10; i++) {
        char b[2] = {(char)('0' + (i + 1) % 10), 0};
        keyboard_add18(8 + i * 59, 506, 54, 44, KEY_TEXT18, b);
    }
    static const char *rows[3][3] = {{"qwertzuiop", "asdfghjkl", "yxcvbnm"},
                                     {"qwertyuiop", "asdfghjkl", "zxcvbnm"},
                                     {"azertyuiop", "qsdfghjklm", "wxcvbn"}};
    static const char *symbols[2][30] = {
        {"!", "@", "#", "$", "%",  "^", "&", "*", "(", ")", "-", "_",  "=", "+", "[",
         "]", "{", "}", "/", "\\", ".", ",", ":", ";", "?", "'", "\"", "<", ">", "|"},
        {"€", "£", "¥", "°", "±", "×", "÷", "√", "π", "∞", "~", "`",  "•", "…", "≤",
         "≥", "≠", "²", "³", "µ", ".", ",", ":", ";", "?", "'", "\"", "©", "®", "§"}};
    for (int row = 0; row < 3; row++) {
        int n = editor_ui.keyboard_symbols ? 10 : (int)strlen(rows[language][row]);
        int step = 59;
        int x0 = (600 - (n * step - 5)) / 2;
        for (int i = 0; i < n; i++) {
            char b[8] = {0};
            if (editor_ui.keyboard_symbols)
                snprintf(b, 8, "%s", symbols[editor_ui.keyboard_symbols - 1][row * 10 + i]);
            else {
                b[0] = rows[language][row][i];
            }
            keyboard_add18(x0 + i * step, 558 + row * 60, 54, 52, KEY_TEXT18, b);
        }
    }
    if (!editor_ui.keyboard_symbols) {
        keyboard_add18(8, 678, 54, 52, KEY_SHIFT18, "");
        keyboard_add18(538, 678, 54, 52, KEY_DELETE18, "");
    }
    keyboard_add18(8, 742, 64, 56, KEY_SYMBOL18,
                   editor_ui.keyboard_symbols == 1   ? "#+="
                   : editor_ui.keyboard_symbols == 2 ? "ABC"
                                           : "123");
    const char *langs[] = {"DE", "EN", "FR"};
    keyboard_add18(80, 742, 62, 56, KEY_LANGUAGE18, langs[language]);
    keyboard_add18(150, 742, 272, 56, KEY_SPACE18, "");
    keyboard_add18(430, 742, 62, 56, editor_ui.keyboard_symbols ? KEY_DELETE18 : KEY_TEXT18,
                   editor_ui.keyboard_symbols ? "" : ".");
    keyboard_add18(500, 742, 92, 56, KEY_ENTER18, "");
}
static int keyboard_hit18(int x, int y) {
    keyboard_layout18();
    for (int i = keyboard_count18 - 1; i >= 0; i--) {
        KeyboardKey18 *k = &keyboard_keys18[i];
        if (x >= k->x - 2 && x < k->x + k->w + 3 && y >= k->y - 2 && y < k->y + k->h + 3)
            return i;
    }
    return -1;
}
static int keyboard_secret18(void) {
    return text_input_spec(editor_ui.keyboard_target).secret;
}
static void keyboard_scroll18(void) {
    editor_ui.note_cursor=text_boundary(editor_ui.edit,editor_ui.note_cursor<0?0:(size_t)editor_ui.note_cursor);
    editor_ui.keyboard_view18=text_visible_start(editor_ui.edit,editor_ui.note_cursor,&body,510,keyboard_secret18()?'*':0);
}
static void keyboard_place18(int x) {
    const char *p = editor_ui.edit + editor_ui.keyboard_view18;
    int xx = 28;
    while (*p) {
        const char *q = p;
        unsigned c = glyph(&q);
        int a = glyph_advance(&body, keyboard_secret18() ? '*' : c);
        if (x < xx + a / 2)
            break;
        xx += a;
        p = q;
    }
    editor_ui.note_cursor = p - editor_ui.edit;
    editor_ui.cursor_visible = 1;
    editor_ui.cursor_epoch = extra_millis();
}
static void keyboard_draw18(void) {
    ui_y_offset = 0;
    if (keyboard_multiline()) {
        rect(0, editor_ui.keyboard_target == 60 ? 132 : 54, 600, editor_ui.keyboard_target == 60 ? 370 : 450, 255);
        editor_draw18();
        icon_button(540, 456, IC_CLOSE);
    } else {
        rect(0, 395, 600, 111, 255);
        rect(0, 395, 600, 1, 190);
        drawtext(20, 410, editor_ui.keyboard_title, &small, 490);
        icon_button(540, 398, IC_CLOSE);
        key_outline(18, 452, 564, 49);
        keyboard_scroll18();
        char b[8192], prefix[8192];
        snprintf(b, sizeof(b), "%s", editor_ui.edit + editor_ui.keyboard_view18);
        snprintf(prefix, sizeof(prefix), "%.*s", editor_ui.note_cursor - editor_ui.keyboard_view18,
                 editor_ui.edit + editor_ui.keyboard_view18);
        if (keyboard_secret18()) {
            memset(b, '*', strlen(b));
            memset(prefix, '*', strlen(prefix));
        }
        drawtext_raw(28, 459, b, &body, 535);
        if (editor_ui.cursor_visible)
            text_caret(28 + textwidth_raw(prefix, &body), 459, &body);
    }
    rect(0, 502, 600, 298, 255);
    rect(0, 502, 600, 1, 190);
    keyboard_layout18();
    int frame = (preferences_keyboard_language() * 16 + editor_ui.keyboard_symbols * 2 +
                 editor_ui.keyboard_shift) * 2 + !!options[OPT_ROUNDING];
    if (keyboard_frame18 == frame) {
        memcpy(ui_pixels + 502 * WIDTH, keyboard_keybed18, sizeof(keyboard_keybed18));
        for (int i = 0; i < keyboard_count18; i++) {
            KeyboardKey18 *k = &keyboard_keys18[i];
            region(k->x, k->y, k->w, k->h);
            widget_shape(k->x,k->y,k->w,k->h,RADIUS_MEDIUM,3,0);
        }
    } else {
        for (int i = 0; i < keyboard_count18; i++) {
            KeyboardKey18 *k = &keyboard_keys18[i];
            if(k->y==506) {
                region(k->x,k->y,k->w,k->h);
                widget_shape(k->x,k->y,k->w,k->h,RADIUS_MEDIUM,3,0);
                control_surface(k->x,k->y,k->w,k->h,RADIUS_MEDIUM,155);
                control_surface(k->x+1,k->y+1,k->w-2,k->h-2,RADIUS_MEDIUM-1,255);
            } else button_h(k->x, k->y, k->w, k->h, "", k->action == KEY_SHIFT18 && editor_ui.keyboard_shift);
            if(k->y==506)ink_color=112;
            centered(k->x, k->y, k->w, k->h, k->text,
                     k->action == KEY_TEXT18 && k->y > 550 ? &body : &small);
            ink_color=0;
            int kind = k->action == KEY_SHIFT18    ? IC_SHIFT
                       : k->action == KEY_DELETE18 ? IC_DELETE
                       : k->action == KEY_SPACE18  ? IC_SPACE
                       : k->action == KEY_ENTER18  ? IC_ENTER
                                                   : -1;
            if (kind >= 0)
                icon(k->x + (k->w - 44) / 2, k->y + (k->h - 44) / 2, kind);
        }
        memcpy(keyboard_keybed18, ui_pixels + 502 * WIDTH, sizeof(keyboard_keybed18));
        keyboard_frame18 = frame;
    }
    if (editor_ui.accent_count18) {
        int w = editor_ui.accent_count18 * 54 + 12;
        raised_surface(editor_ui.accent_x18, 436, w, 58);
        for (int i = 0; i < editor_ui.accent_count18; i++)
            button_h(editor_ui.accent_x18 + 6 + i * 54, 442, 50, 46, editor_ui.accent_keys18[i], 0);
    }
}
static int keyboard_accents18(int key) {
    if (key < 0 || editor_ui.keyboard_symbols)
        return 0;
    KeyboardKey18 *k = &keyboard_keys18[key];
    if (k->action != KEY_TEXT18)
        return 0;
    char c = (char)tolower((unsigned char)k->text[0]);
    const char *variants = c == 'a'   ? "aäàáâãåæ"
                           : c == 'e' ? "eéèêë"
                           : c == 'i' ? "iïîíì"
                           : c == 'o' ? "oöœôóòõø"
                           : c == 'u' ? "uüûùú"
                           : c == 'c' ? "cçćč"
                           : c == 's' ? "sßśš"
                           : c == 'n' ? "nñń"
                           : c == 'y' ? "yÿý"
                           : c == 'z' ? "zžźż"
                                      : NULL;
    if (!variants)
        return 0;
    editor_ui.accent_count18 = 0;
    while (*variants && editor_ui.accent_count18 < 10) {
        const char *p = variants;
        utf8_next(&variants);
        int n = variants - p;
        memcpy(editor_ui.accent_keys18[editor_ui.accent_count18], p, n);
        editor_ui.accent_keys18[editor_ui.accent_count18][n] = 0;
        keyboard_case18(editor_ui.accent_keys18[editor_ui.accent_count18]);
        editor_ui.accent_count18++;
    }
    editor_ui.accent_x18 = k->x - 54;
    if (editor_ui.accent_x18 < 8)
        editor_ui.accent_x18 = 8;
    if (editor_ui.accent_x18 + editor_ui.accent_count18 * 54 + 12 > 592)
        editor_ui.accent_x18 = 580 - editor_ui.accent_count18 * 54;
    pressed = -1;
    return 1;
}
static void keyboard_tap18(int x, int y) {
    if (editor_ui.accent_count18) {
        if (y >= 436 && y < 498 && x >= editor_ui.accent_x18 + 6 &&
            x < editor_ui.accent_x18 + 6 + editor_ui.accent_count18 * 54) {
            keyboard_insert(editor_ui.accent_keys18[(x - editor_ui.accent_x18 - 6) / 54]);
            editor_ui.accent_count18 = 0;
            editor_ui.keyboard_shift = 0;
        } else
            editor_ui.accent_count18 = 0;
        return;
    }
    if (x >= 538 && y >= (keyboard_multiline() ? 456 : 398) &&
        y < (keyboard_multiline() ? 500 : 442)) {
        if (keyboard_multiline())
            commit_keyboard();
        else
            editor_ui.keyboard = 0;
        return;
    }
    if (y < 502) {
        if (keyboard_multiline() && y >= (editor_ui.keyboard_target == 60 ? 132 : 54))
            editor_place18(x, y);
        else if (!keyboard_multiline() && y >= 452)
            keyboard_place18(x);
        else if (y < 395) {
            editor_ui.keyboard = 0;
        }
        return;
    }
    int i = keyboard_hit18(x, y);
    if (i < 0)
        return;
    KeyboardKey18 k = keyboard_keys18[i];
    switch (k.action) {
    case KEY_TEXT18:
        keyboard_insert(k.text);
        if (!editor_ui.keyboard_symbols)
            editor_ui.keyboard_shift = 0;
        break;
    case KEY_SHIFT18:
        editor_ui.keyboard_shift = !editor_ui.keyboard_shift;
        break;
    case KEY_DELETE18:
        keyboard_backspace();
        break;
    case KEY_SYMBOL18:
        editor_ui.keyboard_symbols = (editor_ui.keyboard_symbols + 1) % 3;
        editor_ui.accent_count18 = 0;
        break;
    case KEY_LANGUAGE18:
        options[OPT_KEYBOARD_LANGUAGE] = (preferences_keyboard_language() + 1) % 3;
        library_options_save();
        break;
    case KEY_SPACE18:
        keyboard_insert(" ");
        break;
    case KEY_ENTER18:
        if (keyboard_multiline())
            keyboard_insert("\n");
        else
            commit_keyboard();
        break;
    }
}
static int keyboard_intercept18(const Input *in) {
    if (!editor_ui.keyboard || sleeping || dropdown || editor_ui.clip_active)
        return -1;
    if (editor_ui.accent_count18) {
        if (in->kind == INPUT_DOWN)
            editor_ui.accent_opening_touch = 0;
        else if (in->kind == INPUT_TAP || in->kind == INPUT_SWIPE) {
            if (editor_ui.accent_opening_touch &&
                !(in->y >= 442 && in->y < 488 && in->x >= editor_ui.accent_x18 + 6 &&
                  in->x < editor_ui.accent_x18 + 6 + editor_ui.accent_count18 * 54)) {
                editor_ui.accent_opening_touch = 0;
                return 2;
            }
            editor_ui.accent_opening_touch = 0;
            keyboard_tap18(in->x, in->y);
            pressed = -1;
            return 1;
        } else if (in->kind == INPUT_HOLD)
            return 2;
    }
    if (in->kind == INPUT_HOLD) {
        int i = keyboard_hit18(in->x, in->y);
        if (keyboard_accents18(i)) {
            editor_ui.accent_opening_touch = 1;
            pressed = -1;
            return 1;
        }
        if (i >= 0)
            return 2;
        if (!keyboard_multiline() && in->y >= 452 && in->y < 502)
            keyboard_place18(in->x);
        return -1;
    }
    return -1;
}
