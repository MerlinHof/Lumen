/* System widgets shared by applications; hit regions and text metrics stay consistent. */
static void label_at(int x, int y, int w, int h, const char *s, const Font *f) {
    int height, top = ink_top(s, f, &height);
    drawtext_raw(x, y + (h - height) / 2 - top, s, f, w);
}
/* The last visible line is complete UTF-8 and explicitly indicates truncation. */
static void label_ellipsis(int x, int y, int w, int h, const char *text, const Font *font,
                           int more) {
    if (!more && textwidth_raw(text, font) <= w) {
        label_at(x, y, w, h, text, font);
        return;
    }
    char shown[1024];
    const char *p = text, *end = text;
    int used = 0, dots = textwidth_raw("...", font);
    while (*p && *p != '\n') {
        const char *next = p;
        unsigned c = glyph(&next);
        int a = glyph_advance(font, c);
        if (used + a + dots > w || next - text > (int)sizeof(shown) - 4)
            break;
        used += a;
        end = p = next;
    }
    snprintf(shown, sizeof(shown), "%.*s...", (int)(end - text), text);
    label_at(x, y, w, h, shown, font);
}
static void form_label(int x, int y, int w, int h, const char *label) {
    label = tr(label);
    const Font *font = textwidth_raw(label, &small) > w ? &caption : &small;
    label_ellipsis(x, y, w, h, label, font, 0);
}
/* One linear pass, shared by calculator and the system input viewport. */
static int text_visible_start(const char *text, int cursor, const Font *font, int width,
                              unsigned mask) {
    cursor = text_boundary(text, cursor < 0 ? 0 : (size_t)cursor);
    const char *p = text;
    int used = 0;
    while (*p && p - text < cursor) {
        unsigned g = glyph(&p);
        used += glyph_advance(font, mask ? mask : g);
    }
    p = text;
    while (used > width && p - text < cursor) {
        unsigned g = glyph(&p);
        used -= glyph_advance(font, mask ? mask : g);
    }
    return p - text;
}
