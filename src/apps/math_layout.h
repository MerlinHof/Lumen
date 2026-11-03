/* Bounded two-dimensional formula layout; evaluation remains in calc.c. */
typedef struct {
    int kind, a, b, w, up, down;
    char text[40];
} MathBox;
static MathBox mb[128];
static int mn, math_bad;
static const char *mp;
static int math_node(int kind, int a, int b, const char *text, int n) {
    if (mn >= 128) {
        math_bad = 1;
        return 0;
    }
    int i = mn++;
    mb[i] = (MathBox){.kind = kind, .a = a, .b = b};
    if (n > 39)
        n = 39;
    if (text && n > 0) {
        memcpy(mb[i].text, text, n);
        mb[i].text[n] = 0;
    }
    return i;
}
static void math_space(void) {
    while (*mp == ' ')
        mp++;
}
static int math_expression(int level, int depth);
static int math_atom(int depth) {
    if (depth > 24) {
        math_bad = 1;
        return 0;
    }
    math_space();
    const char *start = mp;
    if (*mp == '+' || *mp == '-') {
        int k = *mp++;
        int a = math_atom(depth + 1);
        return math_node(k == '-' ? 'n' : 'u', a, 0, NULL, 0);
    }
    if (*mp == '(') {
        mp++;
        int a = math_expression(0, depth + 1);
        if (*mp == ')')
            mp++;
        else
            math_bad = 1;
        return math_node('(', a, 0, NULL, 0);
    }
    if (isdigit((unsigned char)*mp) || *mp == '.') {
        while (isdigit((unsigned char)*mp) || *mp == '.')
            mp++;
        return math_node('t', 0, 0, start, mp - start);
    }
    if (isalpha((unsigned char)*mp)) {
        while (isalpha((unsigned char)*mp))
            mp++;
        int n = mp - start;
        char name[40];
        if (n > 39)
            n = 39;
        memcpy(name, start, n);
        name[n] = 0;
        if (*mp == '(') {
            mp++;
            int a = math_expression(0, depth + 1);
            if (*mp == ')')
                mp++;
            else
                math_bad = 1;
            return math_node(!strcmp(name, "sqrt") ? 'r' : 'f', a, 0, name, n);
        }
        return math_node('t', 0, 0, !strcmp(name, "pi") ? "π" : name, !strcmp(name, "pi") ? 2 : n);
    }
    math_bad = 1;
    if (*mp)
        mp++;
    return math_node('t', 0, 0, "?", 1);
}
static int math_expression(int level, int depth) {
    if (depth > 24) {
        math_bad = 1;
        return 0;
    }
    int a = level == 2 ? math_atom(depth + 1) : math_expression(level + 1, depth + 1);
    while (!math_bad) {
        math_space();
        char c = *mp;
        int match = level == 0   ? (c == '+' || c == '-')
                    : level == 1 ? (c == '*' || c == '/')
                                 : c == '^';
        if (!match)
            break;
        mp++;
        int b = level == 2 ? math_expression(2, depth + 1) : math_expression(level + 1, depth + 1);
        if (c == '^' && mb[b].kind == '(')
            b = mb[b].a;
        a = math_node(c, a, b, NULL, 0);
    }
    return a;
}
static const Font *math_font(int smalltext) { return smalltext ? &small : &body; }
static int math_baseline(const Font *f) {
    const unsigned char *p = glyph_pixels(f, '0');
    int b = 0;
    for (int y = 0; y < f->h; y++)
        for (int x = 0; x < f->w; x++)
            if (p[y * f->w + x] < 180)
                b = y + 1;
    return b;
}
static int math_op_width(int kind, const Font *f) {
    return textwidth_raw(kind == '*' ? "·" : kind == '-' ? "−" : "+", f);
}
static void math_parenthesis(int x, int top, int bottom, int right, const Font *f) {
    const char *symbol = right ? ")" : "(";
    int h, ink = ink_top(symbol, f, &h);
    if (bottom - top <= h + 6) {
        drawtext_raw(x, top + (bottom - top - h) / 2 - ink, symbol, f,
                     glyph_advance(f, right ? ')' : '(') + 4);
        return;
    }
    int w = 3;
    int px = x + (right ? 0 : w), py = top;
    for (int i = 1; i <= 48; i++) {
        double t = i / 48.;
        int nx = x + (right ? w * sin(t * 3.14159265359) : w - w * sin(t * 3.14159265359)),
            ny = top + (bottom - top) * t;
        stroke(px, py, nx, ny, 2);
        px = nx;
        py = ny;
    }
}
static void math_measure(int i, int smalltext) {
    if (math_bad || i < 0 || i >= mn)
        return;
    MathBox *n = &mb[i];
    const Font *f = math_font(smalltext);
    if (n->kind == 't') {
        n->w = textwidth(n->text, f);
        int top = f->h, bottom = 0;
        const char *q = n->text;
        while (*q) {
            unsigned c = glyph(&q);
            const unsigned char *p = glyph_pixels(f, c);
            for (int yy = 0; yy < f->h; yy++)
                for (int xx = 0; xx < f->w; xx++)
                    if (p[yy * f->w + xx] < 200) {
                        if (yy < top)
                            top = yy;
                        if (yy + 1 > bottom)
                            bottom = yy + 1;
                    }
        }
        int baseline = math_baseline(f);
        n->up = baseline - top;
        n->down = bottom - baseline;
        if (n->up < 0)
            n->up = 0;
        if (n->down < 0)
            n->down = 0;
        return;
    }
    math_measure(n->a, smalltext);
    MathBox *a = &mb[n->a], *b = &mb[n->b];
    if (n->kind == '/' || n->kind == '^' || n->kind == '+' || n->kind == '-' || n->kind == '*')
        math_measure(n->b, n->kind == '^' ? 1 : smalltext);
    int op = math_op_width(n->kind, f), gap = smalltext ? 4 : 6;
    if (n->kind == '/') {
        n->w = (a->w > b->w ? a->w : b->w) + 16;
        n->up = a->up + a->down + 8;
        n->down = b->up + b->down + 8;
    } else if (n->kind == '^') {
        n->w = a->w + b->w + 3;
        n->up = a->up * 2 / 3 + b->up + b->down + 4;
        n->down = a->down;
    } else if (n->kind == 'r') {
        n->w = a->w + 26;
        n->up = a->up + 7;
        n->down = a->down + 2;
    } else if (n->kind == '(') {
        n->w = a->w + 2 * glyph_advance(f, '(') + 8;
        n->up = a->up;
        n->down = a->down;
    } else if (n->kind == 'f') {
        n->w = textwidth_raw(n->text, f) + 4 + a->w + 2 * glyph_advance(f, '(') + 8;
        n->up = a->up;
        n->down = a->down;
    } else if (n->kind == 'n' || n->kind == 'u') {
        n->w = a->w + op;
        n->up = a->up;
        n->down = a->down;
    } else {
        n->w = a->w + b->w + op + 2 * gap;
        n->up = a->up > b->up ? a->up : b->up;
        n->down = a->down > b->down ? a->down : b->down;
    }
}
static void math_paint(int i, int x, int baseline, int smalltext) {
    MathBox *n = &mb[i], *a = &mb[n->a], *b = &mb[n->b];
    const Font *f = math_font(smalltext);
    int op = math_op_width(n->kind, f), gap = smalltext ? 4 : 6;
    if (n->kind == 't') {
        drawtext_raw(x, baseline - math_baseline(f), n->text, f, n->w + 4);
        return;
    }
    if (n->kind == '/') {
        math_paint(n->a, x + (n->w - a->w) / 2, baseline - 7 - a->down, smalltext);
        stroke(x + 3, baseline, x + n->w - 3, baseline, 2);
        math_paint(n->b, x + (n->w - b->w) / 2, baseline + 8 + b->up, smalltext);
    } else if (n->kind == '^') {
        math_paint(n->a, x, baseline, smalltext);
        math_paint(n->b, x + a->w + 3, baseline - a->up * 2 / 3 - b->down - 4, 1);
    } else if (n->kind == 'r') {
        stroke(x, baseline - 5, x + 6, baseline - 1, 2);
        stroke(x + 6, baseline - 1, x + 11, baseline + a->down, 2);
        stroke(x + 11, baseline + a->down, x + 20, baseline - a->up - 5, 2);
        stroke(x + 20, baseline - a->up - 5, x + n->w, baseline - a->up - 5, 2);
        math_paint(n->a, x + 24, baseline, smalltext);
    } else if (n->kind == '(' || n->kind == 'f') {
        int prefix = n->kind == 'f' ? textwidth_raw(n->text, f) + 4 : 0;
        if (prefix)
            drawtext_raw(x, baseline - math_baseline(f), n->text, f, prefix);
        x += prefix;
        int top = baseline - a->up - 3, bot = baseline + a->down + 3;
        int bw = glyph_advance(f, '(');
        math_parenthesis(x, top, bot, 0, f);
        math_paint(n->a, x + bw + 4, baseline, smalltext);
        math_parenthesis(x + bw + a->w + 8, top, bot, 1, f);
    } else if (n->kind == 'n' || n->kind == 'u') {
        drawtext_raw(x, baseline - math_baseline(f), n->kind == 'n' ? "−" : "+", f, op);
        math_paint(n->a, x + op, baseline, smalltext);
    } else {
        math_paint(n->a, x, baseline, smalltext);
        char symbol[2] = {(char)n->kind, 0};
        drawtext_raw(x + a->w + gap, baseline - math_baseline(f),
                     n->kind == '*'   ? "·"
                     : n->kind == '-' ? "−"
                                      : symbol,
                     f, op);
        math_paint(n->b, x + a->w + op + 2 * gap, baseline, smalltext);
    }
}
static void math_formula_content(int x, int y, int w, int h) {
    mn = math_bad = 0;
    mp = calc_expr;
    if (!*mp)
        return;
    int root = math_expression(0, 0);
    math_space();
    if (*mp)
        math_bad = 1;
    if (!math_bad) {
        math_measure(root, 0);
        if (mb[root].w <= w && mb[root].up + mb[root].down <= h) {
            math_paint(root, x, y + (h - mb[root].up - mb[root].down) / 2 + mb[root].up, 0);
            return;
        }
    }
    if (!math_bad) {
        math_measure(root, 1);
        if (mb[root].w <= w && mb[root].up + mb[root].down <= h) {
            math_paint(root, x, y + (h - mb[root].up - mb[root].down) / 2 + mb[root].up, 1);
            return;
        }
    }
    char text[2048];
    calc_math(text, sizeof(text));
    drawtext_raw(x, y + (h - body.h) / 2, text, &body, w);
}

static void math_formula(int x, int y, int w, int h) {
    int old = ink_color;
    ink_color = -1;
    math_formula_content(x, y, w, h);
    ink_color = old;
}
