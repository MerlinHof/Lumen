/* Shared UTF-8 line layout for variable-height rows. Measures and draws the same
 * ranges, so multiline completion strokes stay aligned with the actual text. */
typedef struct {
    int start, end;
} FlowLine;
static int text_flow(const char *text, const Font *font, int width, FlowLine *lines, int max) {
    int count = 0;
    const char *p = text;
    while (*p && count < max) {
        const char *start = p, *last_space = NULL;
        int used = 0;
        while (*p && *p != '\n') {
            const char *next = p;
            unsigned c = glyph(&next);
            int advance = glyph_advance(font, c);
            if (used + advance > width && p > start)
                break;
            if (*p == ' ')
                last_space = p;
            used += advance;
            p = next;
        }
        if (*p && *p != '\n' && last_space && last_space > start)
            p = last_space;
        const char *end = p;
        while (end > start && end[-1] == ' ')
            end--;
        lines[count++] = (FlowLine){start - text, end - text};
        if (*p == '\n')
            p++;
        else
            while (*p == ' ')
                p++;
    }
    if (!count)
        lines[count++] = (FlowLine){0, 0};
    return count;
}
static void text_flow_draw(int x, int y, int width, const char *text, const Font *font, int step,
                           int max, int strike) {
    FlowLine lines[32];
    if (max > 32)
        max = 32;
    int n = text_flow(text, font, width, lines, max);
    for (int i = 0; i < n; i++) {
        char b[1024];
        snprintf(b, sizeof(b), "%.*s", lines[i].end - lines[i].start, text + lines[i].start);
        label_at(x, y + i * step, width, step, b, font);
        if (strike)
            stroke(x, y + i * step + step / 2, x + textwidth_raw(b, font), y + i * step + step / 2,
                   2);
    }
}
