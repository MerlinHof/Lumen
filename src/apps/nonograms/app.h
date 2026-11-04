/* App-owned module; assembled by apps/host/compose.h. */
static int nono_run_patterns(unsigned mask, unsigned *out) {
    unsigned n = 0, len = 0;
    for (int x = 0; x < 5; x++) {
        if (mask & (1u << x))
            len++;
        else if (len) {
            out[n++] = len;
            len = 0;
        }
    }
    if (len)
        out[n++] = len;
    return n;
}
static int nono_match(unsigned a, unsigned b) {
    unsigned x[5] = {0}, y[5] = {0};
    int n = nono_run_patterns(a, x), m = nono_run_patterns(b, y);
    return n == m && !memcmp(x, y, n * sizeof(unsigned));
}
static int nono_solve(int row, unsigned acc[5], unsigned target[5], int limit) {
    if (row == 5) {
        for (int c = 0; c < 5; c++) {
            unsigned a = 0, b = 0;
            for (int r = 0; r < 5; r++) {
                a |= ((acc[r] >> c) & 1) << r;
                b |= ((target[r] >> c) & 1) << r;
            }
            if (!nono_match(a, b))
                return 0;
        }
        return 1;
    }
    int count = 0;
    for (unsigned m = 0; m < 32; m++)
        if (nono_match(m, target[row])) {
            acc[row] = m;
            count += nono_solve(row + 1, acc, target, limit - count);
            if (count >= limit)
                return count;
        }
    return count;
}
static void v1_nono_new(void) {
    unsigned t[5], a[5];
    int found = 0;
    for (int tries = 0; tries < 200; tries++) {
        for (int r = 0; r < 5; r++)
            t[r] = (pro_random() >> 12) & 31;
        if (nono_solve(0, a, t, 2) == 1) {
            found = 1;
            break;
        }
    }
    if (!found)
        for (int i = 0; i < 5; i++)
            t[i] = 31;
    for (int i = 0; i < 25; i++)
        nono_solution[i] = (t[i / 5] >> (i % 5)) & 1;
    memset(nono_cells, 0, 25);
    nono_ready = 1;
    v1_dirty = 1;
}
static void v1_nono_draw(void) {
    heading_actions("Nonogramme", 1);
    icon_button(528, 74, IC_RESTART);
    centered(28, 133, 544, 32, "5 × 5 · Ein Bild aus Logik", ui_font(UI_SECONDARY));
    for (int r = 0; r < 5; r++) {
        unsigned mask = 0, v[5];
        for (int c = 0; c < 5; c++)
            mask |= nono_solution[r * 5 + c] << c;
        int n = nono_run_patterns(mask, v);
        char b[64] = "";
        for (int i = 0; i < n; i++) {
            char x[8];
            snprintf(x, 8, "%u ", v[i]);
            strcat(b, x);
        }
        if (!n)
            strcpy(b, "0");
        centered(18, 263 + r * 76, 126, 76, b, ui_font(UI_PRIMARY));
    }
    for (int c = 0; c < 5; c++) {
        unsigned mask = 0, v[5];
        for (int r = 0; r < 5; r++)
            mask |= nono_solution[r * 5 + c] << r;
        int n = nono_run_patterns(mask, v);
        for (int j = 0; j < n; j++) {
            char b[8];
            snprintf(b, 8, "%u", v[j]);
            centered(146 + c * 76, 243 - (n - j) * 30, 76, 30, b, ui_font(UI_BODY));
        }
    }
    int win = 1;
    for (int i = 0; i < 25; i++) {
        int x = 146 + (i % 5) * 76, y = 263 + (i / 5) * 76;
        region(x, y, 76, 76);
        rect(x + 2, y + 2, 72, 72, nono_cells[i] == 1 ? 0 : 255);
        key_outline(x, y, 76, 76);
        if (nono_cells[i] == 2) {
            stroke(x + 24, y + 24, x + 52, y + 52, 2);
            stroke(x + 52, y + 24, x + 24, y + 52, 2);
        }
        if ((nono_cells[i] == 1) != nono_solution[i])
            win = 0;
    }
    const char *tabs[] = {"Füllen", "Leer markieren"};
    segments(28, 692, 544, 48, tabs, 2, nono_mark);
    if (win)
        centered(28, 746, 544, 40, "Gelöst!", ui_font(UI_PRIMARY));
}
