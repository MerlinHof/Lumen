/* Shared plain-line measurement for calendar note excerpts. Rich Markdown uses the Reader. */
static const char *md_next(const char *p, int width, const Font *f) {
    const char *q = p, *space = NULL;
    int n = 0;
    while (*q && *q != '\n') {
        const char *old = q;
        unsigned c = glyph(&q);
        int a = glyph_advance(f, c);
        if (n + a > width) {
            q = space && space > p ? space : old;
            break;
        }
        n += a;
        if (c == ' ')
            space = q;
    }
    if (q == p && *q && *q != '\n')
        glyph(&q);
    if (*q == '\n')
        q++;
    return q;
}
