/* App-owned module; assembled by apps/host/compose.h. */
static void suit_icon(int x, int y, int suit) {
    if (suit == 0) {
        disc(x - 4, y - 3, 4, 0);
        disc(x + 4, y - 3, 4, 0);
        stroke(x - 7, y - 2, x, y + 7, 5);
        stroke(x + 7, y - 2, x, y + 7, 5);
    } else if (suit == 1) {
        stroke(x, y - 8, x + 6, y, 3);
        stroke(x + 6, y, x, y + 8, 3);
        stroke(x, y + 8, x - 6, y, 3);
        stroke(x - 6, y, x, y - 8, 3);
    } else if (suit == 2) {
        disc(x, y - 4, 4, 0);
        disc(x - 4, y + 2, 4, 0);
        disc(x + 4, y + 2, 4, 0);
        stroke(x, y + 3, x, y + 9, 3);
    } else {
        stroke(x, y - 8, x - 5, y, 5);
        stroke(x, y - 8, x + 5, y, 5);
        disc(x - 3, y + 1, 4, 0);
        disc(x + 3, y + 1, 4, 0);
        stroke(x, y + 2, x, y + 9, 3);
    }
}
static void card(int x, int y, int id, int hidden, int selected) {
    rect(x, y, 72, 96, 255);
    key_outline(x, y, 72, 96);
    if (hidden) {
        for (int j = 8; j < 88; j += 8)
            for (int i = 8; i < 65; i += 8)
                disc(x + i, y + j, 1, 125);
        return;
    }
    if (id < 0)
        return;
    int rank = id % 13 + 1, suit = id / 13;
    char b[8];
    snprintf(b, sizeof(b),
             rank == 1    ? "A"
             : rank == 11 ? "J"
             : rank == 12 ? "Q"
             : rank == 13 ? "K"
                          : "%d",
             rank);
    ink_color = suit < 2 ? 85 : 0;
    drawtext_raw(x + 6, y + 4, b, ui_font(UI_BODY), 40);
    ink_color = 0;
    suit_icon(x + 54, y + 17, suit);
    suit_icon(x + 36, y + 58, suit);
    if (selected) {
        rect(x, y, 72, 4, 0);
        rect(x, y + 92, 72, 4, 0);
    }
}
static int column_gap(int c) {
    int n = solitaire.state.count[c];
    return n > 1 ? ((350 / (n - 1)) < 27 ? 350 / (n - 1) : 27) : 27;
}
static void app_solitaire(void) {
    heading_actions("Solitaire", 4);
    icon_button(408, 74, IC_UNDO);
    icon_button(468, 74, IC_DONE);
    icon_button(528, 74, IC_RESTART);
    PatienceState *s = &solitaire.state;
    region(21, 132, 72, 96);
    card(21, 132, -1, s->ns > 0, 0);
    if (!s->ns)
        icon(35, 158, IC_RESTART);
    region(101, 132, 72, 96);
    card(101, 132, s->nw ? s->waste[s->nw - 1] : -1, 0, pat_source == -1);
    for (int i = 0; i < 4; i++) {
        region(261 + i * 80, 132, 72, 96);
        card(261 + i * 80, 132, s->foundation[i] ? i * 13 + s->foundation[i] - 1 : -1, 0,
             pat_source == 7 + i);
        if (!s->foundation[i])
            suit_icon(297 + i * 80, 180, i);
    }
    for (int c = 0; c < 7; c++) {
        int x = 21 + c * 80, gap = column_gap(c);
        if (!s->count[c]) {
            key_outline(x, 248, 72, 96);
            region(x, 248, 72, 96);
        }
        for (int i = 0; i < s->count[c]; i++) {
            int y = 248 + i * gap;
            card(x, y, s->column[c][i], i < s->hidden[c], pat_source == c && pat_index == i);
            region(x, y, 72, i == s->count[c] - 1 ? 96 : gap);
        }
    }
    char b[100];
    snprintf(b, sizeof(b), "%s %u · %s %d", tr("Züge"), s->moves, tr("Stapel"), s->ns);
    drawtext_raw(28, 736, b, ui_font(UI_SECONDARY), 544);
    if (patience_won(&solitaire))
        centered(28, 680, 544, 50, "Geschafft!", ui_font(UI_TITLE));
    else
        drawtext(28, 768, "Karte wählen, dann Ziel antippen. Haken: automatisch ablegen.", ui_font(UI_SECONDARY),
                 544);
}
