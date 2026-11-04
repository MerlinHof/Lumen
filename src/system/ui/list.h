/* Shared paper-list rhythm. Coordinates describe visible ink, never a font's
 * invisible cell padding. Apps own data and actions; this owns row geometry. */
enum { UI_MARGIN = 28, UI_CONTENT = 544, UI_DIVIDER = 205 };
typedef struct {
    int x, title_y, title_h, detail_y, detail_h, width, icon_y, action_y;
} ListLayout;
static void divider(int x, int y, int w) {
    rect(x, y, w, 1, UI_DIVIDER);
}
static ListLayout list_layout(int x, int y, int w, int h, int leading, int trailing, int lines,
                              int detail, const Font *font) {
    int line_height;
    ink_top("Ag", font, &line_height);
    int primary = (lines - 1) * 25 + line_height, secondary = detail ? 16 : 0;
    int group = primary + (detail ? 6 + secondary : 0);
    int top = y + (h - 1 - group) / 2;
    return (ListLayout){
        x + (leading ? 56 : 2),
        top,
        primary,
        top + primary + 6,
        secondary,
        w - (leading ? 56 : 2) - trailing - 10,
        (top + (line_height - 44) / 2 < y + 4 ? y + 4 : top + (line_height - 44) / 2),
        y + (h - 44) / 2};
}
static int list_text(int x, int y, int width, const char *text, const Font *font, int limit) {
    FlowLine lines[4];
    if (limit > 4)
        limit = 4;
    int n = text_flow(text, font, width, lines, limit);
    int line_height;
    ink_top("Ag", font, &line_height);
    for (int i = 0; i < n; i++) {
        char b[1024];
        snprintf(b, sizeof(b), "%.*s", lines[i].end - lines[i].start, text + lines[i].start);
        const char *rest = text + lines[i].end;
        while (*rest == ' ' || *rest == '\n')
            rest++;
        label_ellipsis(x, y + i * 25, width, line_height, b, font, i == n - 1 && *rest);
    }
    return n;
}
static ListLayout list_row(int x, int y, int w, int h, int leading, int trailing,
                           const char *title_text, const char *detail, const Font *font,
                           int lines) {
    int available = w - (leading != -1 ? 56 : 2) - trailing - 10;
    FlowLine flow[4];
    int count = text_flow(title_text, font, available, flow, lines > 4 ? 4 : lines);
    ListLayout l = list_layout(x, y, w, h, leading != -1, trailing, count, detail && *detail, font);
    region(x, y, w, h - 1);
    list_text(l.x, l.title_y, l.width, title_text, font, count);
    if (detail && *detail) {
        int old = ink_color;
        ink_color = 112;
        label_ellipsis(l.x, l.detail_y, l.width, l.detail_h, detail, &caption, 0);
        ink_color = old;
    }
    if (leading >= 0)
        icon(x, l.icon_y, leading);
    divider(x, y + h - 1, w);
    return l;
}
