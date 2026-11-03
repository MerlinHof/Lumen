static int text_editable_hit(const Input *in) {
    if (keyboard)
        return keyboard_multiline() ? (in->y >= 54 && in->y < 458) : (in->y >= 452 && in->y < 501);
    if (screen == 27)
        return in->x >= 28 && in->x < 514 && in->y >= 361 && in->y < 413;
    return screen == 30 && !note_ink_mode && in->y >= 54 && in->y < 742;
}
/* Shared editable-text clipboard and word selection. UI buttons never become selection targets. */
typedef struct {
    int x, y, w, h;
    const Font *f;
    char text[1024];
} TextRun;
static TextRun text_runs[192];
static int text_run_count, clip_s, clip_e, clip_edit_start = -1, clip_run = -1;
static char clipboard_text[8192], clip_source[8192];
static void text_capture_record(int x, int y, const char *s, const Font *f) {
    if (text_run_count >= 192 || clip_active || !s || !*s || y < 50 || y > 740)
        return;
    TextRun *r = &text_runs[text_run_count++];
    r->x = x;
    r->y = y;
    r->f = f;
    r->h = f->h;
    r->w = textwidth_raw(s, f);
    snprintf(r->text, sizeof(r->text), "%s", s);
}
static void text_capture_begin(void) {
    if (!clip_active)
        text_run_count = 0;
}
static void clipboard_draw(void) {
    if (!clip_active)
        return;
    if (clip_run >= 0 && clip_run < text_run_count) {
        TextRun *r = &text_runs[clip_run];
        char b[1024];
        snprintf(b, sizeof(b), "%.*s", clip_s, clip_source);
        int left = r->x + textwidth_raw(b, r->f);
        snprintf(b, sizeof(b), "%.*s", clip_e, clip_source);
        int right = r->x + textwidth_raw(b, r->f);
        stroke(left, r->y + r->h - 3, right, r->y + r->h - 3, 2);
        disc(left, r->y + r->h, 4, 0);
        disc(right, r->y + r->h, 4, 0);
    }
    rect(23, 250, 558, 258, 170);
    rect(19, 246, 558, 258, 255);
    key_outline(19, 246, 558, 258);
    char b[1024];
    int n = clip_e - clip_s;
    if (n < 0)
        n = 0;
    if (n > 1023)
        n = 1023;
    memcpy(b, clip_source + clip_s, n);
    b[n] = 0;
    wrapped_raw(35, 264, b, &small, 526, 3);
    button_h(35, 362, 165, 48, "Kopieren", 0);
    button_h(217, 362, 165, 48, "Alles", 0);
    button_h(399, 362, 160, 48, "Schließen", 0);
    button_h(35, 435, 250, 48, "Ausschneiden", 0);
    button_h(310, 435, 250, 48, "Einfügen", 0);
}
static char *clipboard_edit_buffer18(void) {
    return keyboard ? edit : screen == 27 ? calc_expr : NULL;
}
static int *clipboard_edit_cursor18(void) {
    return keyboard ? &note_cursor : &calc_cursor;
}
static void clipboard_paste(void) {
    if (!clipboard_edit_buffer18() || !*clipboard_text) {
        ui_message("Zum Einfügen zuerst ein Textfeld öffnen.");
        return;
    }
    if (!text_insert(clipboard_edit_buffer18(), keyboard ? keyboard_capacity() : sizeof(calc_expr),
                     clipboard_edit_cursor18(), clipboard_text))
        ui_message("Text passt nicht in dieses Feld.");
}
static int clipboard_input(const Input *in) {
    if (screen == 2 && selecting == 2 && in->kind == INPUT_TAP && in->x >= 390 && in->y >= 628 &&
        in->y < 664) {
        snprintf(clipboard_text, sizeof(clipboard_text), "%s", selection.text);
        selecting = 0;
        ui_message("Kopiert.");
        return 1;
    }
    if (clip_active) {
        if (in->kind == INPUT_MOVE && clip_run >= 0) {
            TextRun *r = &text_runs[clip_run];
            const char *p = clip_source;
            int xx = r->x;
            while (*p) {
                const char *n = p;
                unsigned c = glyph(&n);
                if (in->x < xx + glyph_advance(r->f, c) / 2)
                    break;
                xx += glyph_advance(r->f, c);
                p = n;
            }
            int end = p - clip_source;
            if (end > clip_s)
                clip_e = end;
            return 1;
        }
        if (in->kind == INPUT_TAP) {
            int x = in->x, y = in->y;
            if (y >= 362 && y < 410) {
                if (x < 205) {
                    int n = clip_e - clip_s;
                    if (n > 8191)
                        n = 8191;
                    memcpy(clipboard_text, clip_source + clip_s, n);
                    clipboard_text[n] = 0;
                    clip_active = 0;
                    ui_message("Kopiert.");
                } else if (x < 390) {
                    if (screen == 30 || (keyboard && keyboard_target == 30)) {
                        snprintf(clip_source, sizeof(clip_source), "%s",
                                 keyboard ? edit : note_text);
                        clip_edit_start = 0;
                    } else if (clipboard_edit_buffer18()) {
                        snprintf(clip_source, sizeof(clip_source), "%s", clipboard_edit_buffer18());
                        clip_edit_start = 0;
                    }
                    clip_run = -1;
                    clip_s = 0;
                    clip_e = strlen(clip_source);
                } else
                    clip_active = 0;
            } else if (y >= 435 && y < 483) {
                if (x < 300) {
                    if (clipboard_edit_buffer18() && clip_edit_start >= 0) {
                        int n = clip_e - clip_s;
                        memcpy(clipboard_text, clip_source + clip_s, n);
                        clipboard_text[n] = 0;
                        int start = clip_edit_start + clip_s, end = start + n;
                        if (end <= (int)strlen(clipboard_edit_buffer18()))
                            text_erase(clipboard_edit_buffer18(), clipboard_edit_cursor18(), start,
                                       end);
                    } else
                        ui_message("Ausschneiden nur im Texteditor.");
                } else
                    clipboard_paste();
                clip_active = 0;
            }
        }
        return 1;
    }
    if (in->kind != INPUT_HOLD || screen == 2 || in->y < 50)
        return 0;
    if (!text_editable_hit(in))
        return 0;
    if (clipboard_edit_buffer18()) {
        if (keyboard && (keyboard_target == 5 || keyboard_target == 32 || keyboard_target == 176))
            return 0;
        snprintf(clip_source, sizeof(clip_source), "%s", clipboard_edit_buffer18());
        clip_edit_start = 0;
        clip_run = -1;
        const char *source = clipboard_edit_buffer18();
        clip_s = text_boundary(source, *clipboard_edit_cursor18());
        clip_e = clip_s;
        while (clip_s > 0 && source[clip_s - 1] != ' ' && source[clip_s - 1] != '\n')
            clip_s--;
        while (source[clip_e] && source[clip_e] != ' ' && source[clip_e] != '\n')
            clip_e++;
        clip_s = text_boundary(source, clip_s);
        if (clip_s == clip_e) {
            clip_s = 0;
            clip_e = strlen(clipboard_edit_buffer18());
        }
        clip_active = 1;
        pressed = -1;
        return 1;
    }
    clip_source[0] = 0;
    clip_edit_start = -1;
    for (int i = text_run_count - 1; i >= 0; i--) {
        TextRun *r = &text_runs[i];
        if (in->x >= r->x && in->x <= r->x + r->w && in->y >= r->y && in->y < r->y + r->h) {
            snprintf(clip_source, sizeof(clip_source), "%s", r->text);
            int px = r->x, pos = 0;
            const char *p = r->text;
            while (*p) {
                const char *old = p;
                unsigned g = glyph(&p);
                int a = glyph_advance(r->f, g);
                if (in->x < px + a) {
                    pos = old - r->text;
                    break;
                }
                px += a;
                pos = p - r->text;
            }
            clip_s = pos;
            clip_e = pos;
            while (clip_s > 0 && clip_source[clip_s - 1] != ' ' && clip_source[clip_s - 1] != '\n')
                clip_s--;
            while (clip_source[clip_e] && clip_source[clip_e] != ' ' && clip_source[clip_e] != '\n')
                clip_e++;
            if (clip_s == clip_e) {
                clip_s = 0;
                clip_e = strlen(clip_source);
            }
            if (keyboard) {
                char *found = strstr(edit, clip_source);
                if (found)
                    clip_edit_start = found - edit;
            }
            clip_run = i;
            clip_active = 1;
            return 1;
        }
    }
    return 0;
}
