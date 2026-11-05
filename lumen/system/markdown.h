/* Safe shared Markdown-to-HTML rendering for reader documents and exports.
 * Literal HTML is escaped; links permit only https/http anchors. */
static void md_escape17(FILE *f, const char *s, int n) {
    for (int i = 0; i < n; i++) {
        unsigned char c = s[i];
        if (c == '&')
            fputs("&amp;", f);
        else if (c == '<')
            fputs("&lt;", f);
        else if (c == '>')
            fputs("&gt;", f);
        else if (c == '\"')
            fputs("&quot;", f);
        else
            fputc(c, f);
    }
}
static void md_inline17(FILE *f, const char *s, int n) {
    int bold = 0, em = 0, code = 0;
    for (int i = 0; i < n;) {
        if (s[i] == '\\' && i + 1 < n) {
            md_escape17(f, s + i + 1, 1);
            i += 2;
            continue;
        }
        if (s[i] == '`') {
            fputs(code ? "</code>" : "<code>", f);
            code ^= 1;
            i++;
            continue;
        }
        if (!code && i + 1 < n && s[i] == '*' && s[i + 1] == '*') {
            fputs(bold ? "</strong>" : "<strong>", f);
            bold ^= 1;
            i += 2;
            continue;
        }
        if (!code && s[i] == '*') {
            fputs(em ? "</em>" : "<em>", f);
            em ^= 1;
            i++;
            continue;
        }
        if (!code && s[i] == '[') {
            int end = i + 1;
            while (end < n && s[end] != ']')
                end++;
            if (end + 2 < n && s[end + 1] == '(') {
                int close = end + 2;
                while (close < n && s[close] != ')')
                    close++;
                if (close < n &&
                    (!strncmp(s + end + 2, "https://", 8) || !strncmp(s + end + 2, "http://", 7))) {
                    fputs("<a href=\"", f);
                    md_escape17(f, s + end + 2, close - end - 2);
                    fputs("\">", f);
                    md_escape17(f, s + i + 1, end - i - 1);
                    fputs("</a>", f);
                    i = close + 1;
                    continue;
                }
            }
        }
        md_escape17(f, s + i++, 1);
    }
    if (code)
        fputs("</code>", f);
    if (em)
        fputs("</em>", f);
    if (bold)
        fputs("</strong>", f);
}
static void markdown_render17(FILE *f, const char *text) {
    int code = 0, list = 0;
    const char *p = text;
    while (*p) {
        const char *end = strchr(p, '\n');
        if (!end)
            end = p + strlen(p);
        int n = end - p;
        if (n && p[n - 1] == '\r')
            n--;
        if (n >= 3 && !strncmp(p, "```", 3)) {
            if (list) {
                fputs(list == 1 ? "</ul>" : "</ol>", f);
                list = 0;
            }
            fputs(code ? "</pre>" : "<pre>", f);
            code ^= 1;
        } else if (code) {
            md_escape17(f, p, n);
            fputc('\n', f);
        } else {
            int h = 0;
            while (h < 6 && h < n && p[h] == '#')
                h++;
            int offset = 0, type = 0;
            if (n >= 2 && (p[0] == '-' || p[0] == '*' || p[0] == '+') && p[1] == ' ') {
                type = 1;
                offset = 2;
            } else if (n >= 3 && isdigit((unsigned char)p[0])) {
                int k = 0;
                while (k < n && isdigit((unsigned char)p[k]))
                    k++;
                if (k + 1 < n && p[k] == '.' && p[k + 1] == ' ') {
                    type = 2;
                    offset = k + 2;
                }
            }
            if (list && list != type) {
                fputs(list == 1 ? "</ul>" : "</ol>", f);
                list = 0;
            }
            if (type) {
                if (!list) {
                    fputs(type == 1 ? "<ul>" : "<ol>", f);
                    list = type;
                }
                fputs("<li>", f);
                md_inline17(f, p + offset, n - offset);
                fputs("</li>", f);
            } else if (h && h < n && p[h] == ' ') {
                fprintf(f, "<h%d>", h);
                md_inline17(f, p + h + 1, n - h - 1);
                fprintf(f, "</h%d>", h);
            } else if (n >= 2 && p[0] == '>' && p[1] == ' ') {
                fputs("<blockquote>", f);
                md_inline17(f, p + 2, n - 2);
                fputs("</blockquote>", f);
            } else if (n) {
                fputs("<p>", f);
                md_inline17(f, p, n);
                fputs("</p>", f);
            }
        }
        p = *end ? end + 1 : end;
    }
    if (list)
        fputs(list == 1 ? "</ul>" : "</ol>", f);
    if (code)
        fputs("</pre>", f);
}
