/* Native text worksheet: expressions and live results share a line. */
#include "../scratch.h"
static char scratch_text[8192];
static int scratch_ready, scratch_top, scratch_scroll_lock;
static ScratchLine scratch_rows[SCRATCH_LINES];
static const char *scratch_cached_text;
static char scratch_cache[8192];
static int scratch_count;
static int scratch_can_insert(const char *text) {
    int lines = 1;
    for (const char *p = edit; *p; p++)
        lines += *p == '\n';
    for (const char *p = text; *p; p++)
        lines += *p == '\n';
    scratch_scroll_lock = 0;
    return lines <= SCRATCH_LINES;
}
static void scratch_store(const char *text) {
    scratch_top = (scratch_top / 17) * 17;
    char p[2048];
    local_path(p, sizeof(p), "numscratch.txt");
    if (!atomic_write(p, text, strlen(text)))
        ui_message("Speichern fehlgeschlagen.");
    snprintf(scratch_text, sizeof(scratch_text), "%s", text);
}
static void scratch_open(void) {
    if (!scratch_ready) {
        char p[2048];
        local_path(p, sizeof(p), "numscratch.txt");
        if (access(p, F_OK) == 0)
            extra_load_text("numscratch.txt", scratch_text, sizeof(scratch_text));
        else
            snprintf(scratch_text, sizeof(scratch_text),
                     "# NumScratch\nprice = 120\ntax = price * 19%%\nprice + tax\n\n# "
                     "Units\ndistance = 100 km\ndistance to miles\n\n# Lists\nexpenses = [10, 20, "
                     "30]\navg expenses\nsum expenses\n\n# Continue\n100 + 50\n* 5\nsqrt(144)\n");
        scratch_ready = 1;
    }
    scratch_top = 0;
    scratch_scroll_lock = 0;
}
static void scratch_calculate(const char *text) {
    if (!scratch_cached_text || strcmp(scratch_cache, text)) {
        snprintf(scratch_cache, sizeof(scratch_cache), "%s", text);
        scratch_count = scratch_evaluate(scratch_cache, scratch_rows, SCRATCH_LINES);
        scratch_cached_text = scratch_cache;
    }
}
static int scratch_cursor_line(void) {
    int line = 0;
    for (int i = 0; edit[i] && i < note_cursor; i++)
        if (edit[i] == '\n')
            line++;
    return line;
}
static void scratch_editor(void) {
    const char *text = keyboard && keyboard_target == 60 ? edit : scratch_text;
    int editing = keyboard && keyboard_target == 60, visible = editing ? 10 : 17;
    scratch_calculate(text);
    int crow = editing ? scratch_cursor_line() : 0;
    if (editing && !scratch_scroll_lock) {
        if (crow < scratch_top)
            scratch_top = crow;
        if (crow >= scratch_top + visible)
            scratch_top = crow - visible + 1;
    }
    if (scratch_top >= scratch_count)
        scratch_top = scratch_count ? scratch_count - 1 : 0;
    rect(0, 132, 600, editing ? 370 : 604, 255);
    rect(404, 144, 2, editing ? 308 : 560, 205);
    for (int r = 0; r < visible && scratch_top + r < scratch_count; r++) {
        int i = scratch_top + r, y = 145 + r * 32;
        ScratchLine *line = &scratch_rows[i];
        char raw[1024];
        snprintf(raw, sizeof(raw), "%.*s", line->length, text + line->start);
        char number[12];
        snprintf(number, sizeof(number), "%d", i + 1);
        ink_color = 146;
        drawtext_raw(9, y, number, ui_font(UI_SECONDARY), 31);
        int offset = 0;
        if (editing && i == crow) {
            while (offset < note_cursor - line->start &&
                   textwidth_raw(raw + offset, ui_font(UI_BODY)) > 340) {
                const char *p = raw + offset;
                glyph(&p);
                offset = p - raw;
            }
        }
        ink_color = raw[0] == '#' ? 104 : 0;
        drawtext_raw(43, y, raw + offset, ui_font(UI_BODY), 350);
        ink_color = 0;
        if (line->state && !(editing && i == crow && line->state < 0)) {
            ink_color = line->state < 0 ? 118 : 0;
            drawtext_raw(419, y, line->result, ui_font(UI_BODY), 170);
            ink_color = 0;
        }
        if (editing && i == crow && cursor_visible) {
            int len = note_cursor - line->start - offset;
            if (len < 0)
                len = 0;
            if (len > 1023)
                len = 1023;
            char prefix[1024];
            snprintf(prefix, sizeof(prefix), "%.*s", len, raw + offset);
            text_caret(43 + textwidth_raw(prefix, ui_font(UI_BODY)), y, ui_font(UI_BODY));
        }
    }
    if (!editing) {
        int pages = (scratch_count + 16) / 17;
        (void)pages;
    }
}
static void scratch_touch(int x, int y) {
    scratch_calculate(edit);
    int i = scratch_top + (y - 145) / 32;
    if (i < 0)
        i = 0;
    if (i >= scratch_count)
        i = scratch_count - 1;
    ScratchLine *line = &scratch_rows[i];
    int px = 43, at = line->start;
    const char *p = edit + at;
    while (*p && *p != '\n') {
        const char *n = p;
        unsigned g = glyph(&n);
        int w = glyph_advance(ui_font(UI_BODY), g);
        if (x < px + w / 2)
            break;
        px += w;
        p = n;
    }
    note_cursor = p - edit;
    scratch_scroll_lock = 0;
}
static void app_scratch(void) {
    heading_actions("NumScratch", 1);
    icon_button(528, 74, IC_TYPE);
    scratch_editor();
}
static int scratch_input(const Input *in) {
    if (in->kind == INPUT_SWIPE) {
        scratch_top += in->dy > 0 ? -17 : 17;
        if (scratch_top < 0)
            scratch_top = 0;
        if (scratch_top >= scratch_count)
            scratch_top = scratch_count > 17 ? ((scratch_count - 1) / 17) * 17 : 0;
        return 1;
    }
    if (in->kind != INPUT_TAP)
        return 2;
    if (in->y >= 744) {
        if (in->x < 300 && scratch_top >= 17)
            scratch_top -= 17;
        else if (in->x >= 300 && scratch_top + 17 < scratch_count)
            scratch_top += 17;
        return 1;
    }
    if ((in->y >= 132 && in->y < 710) || (in->y >= 74 && in->y < 118 && in->x >= 516)) {
        open_keyboard(60, "NumScratch", scratch_text);
        note_cursor = strlen(edit);
        scratch_scroll_lock = 0;
        if (in->y >= 132)
            scratch_touch(in->x, in->y);
    }
    return 1;
}
