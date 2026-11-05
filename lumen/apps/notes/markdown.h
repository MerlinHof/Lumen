/* App-owned module; assembled by apps/host/compose.h. */
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
static void md_inline(int x, int y, const char *s, int width, const Font *f, int literal) {
    int bold = 0, code = 0;
    const char *p = s;
    while (*p && *p != '\n' && x < 30 + width) {
        if (!literal && p[0] == '*' && p[1] == '*') {
            bold ^= 1;
            p += 2;
            continue;
        }
        if (!literal && *p == '`') {
            code ^= 1;
            p++;
            continue;
        }
        const char *start = p;
        unsigned c = glyph(&p);
        char b[8] = {0};
        size_t n = p - start;
        if (n > 7)
            n = 7;
        memcpy(b, start, n);
        int a = glyph_advance(f, c);
        if (code)
            rect(x, y + 1, a, f->h - 3, 230);
        drawtext_raw(x, y, b, f, a + 4);
        if (bold)
            drawtext_raw(x + 1, y, b, f, a + 3);
        x += a;
    }
}
static void v1_markdown_draw(void) {
    icon_button(528, 57, IC_TYPE);
    const char *p = note_text;
    int pg = 0, y = 116, code = 0;
    while (*p) {
        int level = 0, quote = 0, list = 0;
        const char *q = p;
        if (!strncmp(q, "```", 3)) {
            code ^= 1;
            p = strchr(q, '\n');
            if (!p)
                break;
            p++;
            continue;
        }
        if (!code) {
            while (*q == '#' && level < 6) {
                level++;
                q++;
            }
            if (level && *q == ' ')
                q++;
            else {
                level = 0;
                q = p;
            }
            if (*q == '>' && q[1] == ' ') {
                quote = 1;
                q += 2;
            }
            if (*q == '-' && q[1] == ' ') {
                list = 1;
                q += 2;
            }
        }
        const Font *f = level ? ui_font(UI_TITLE) : ui_font(UI_BODY);
        int h = level ? 44 : 30;
        if (y + h > 731) {
            pg++;
            y = 116;
        }
        int inset = (list || quote) ? 22 : 0;
        const char *next = md_next(q, 540 - inset, f);
        char b[1024];
        snprintf(b, sizeof(b), "%.*s", (int)(next - q), q);
        if (pg == markdown_page) {
            if (code) {
                rect(27, y, 546, h, 235);
                md_inline(34, y, b, 530, ui_font(UI_BODY), 1);
            } else if (!strncmp(b, "---", 3)) {
                rect(30, y + 14, 540, 1, 180);
            } else {
                if (list)
                    disc(35, y + 15, 3, 0);
                if (quote)
                    rect(30, y + 3, 3, h - 5, 140);
                md_inline(30 + inset, y, b, 540 - inset, f, 0);
            }
        }
        p = next;
        y += h;
    }
    markdown_pages = pg + 1;
    if (markdown_page >= markdown_pages)
        markdown_page = markdown_pages - 1;
    pager(746, &markdown_page, markdown_pages);
}
#include "../planetarium/app.h"
