/* App-owned module; assembled by apps/host/compose.h. */
static int v1_word_index(void) {
    time_t now = time(NULL);
    struct tm t;
    localtime_r(&now, &t);
    int y = t.tm_year + 1900, m = t.tm_mon + 1, d = t.tm_mday;
    y -= m <= 2;
    int era = (y >= 0 ? y : y - 399) / 400;
    unsigned yoe = y - era * 400, doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1,
             doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    int total = sizeof(wordle_answers) / sizeof(*wordle_answers),
        base = (era * 146097 + (int)doe) % total, offset = 0;
    char *saved = service_read("wordle-calendar-offset-v11.txt", 32);
    if (saved) {
        offset = atoi(saved);
        if (offset < 0 || offset >= total)
            offset = 0;
        free(saved);
    } else {
        if (wordle_game.date == news_date() && wordle_game.row > 0) {
            int old = (((uint32_t)news_date() * 2654435761u) >> 16) % total;
            offset = (old - base + total) % total;
        }
        char b[24];
        snprintf(b, sizeof(b), "%d", offset);
        extra_write_text("wordle-calendar-offset-v11.txt", b);
    }
    return (base + offset) % total;
}
static int word_valid(const char *w) {
    int l = 0, r = sizeof(wordle_words) / 6;
    while (l < r) {
        int m = (l + r) / 2, c = strcmp(w, wordle_words[m]);
        if (!c)
            return 1;
        if (c > 0)
            l = m + 1;
        else
            r = m;
    }
    return 0;
}
static void v1_word_draw(void) {
    heading("Wordle");
    char b[80];
    snprintf(b, sizeof(b), "%02d.%02d.%04d · %s", wordle_game.date % 100,
             wordle_game.date / 100 % 100, wordle_game.date / 10000, tr("English · 6 Versuche"));
    centered(28, 130, 544, 35, b, ui_font(UI_SECONDARY));
    for (int r = 0; r < 6; r++)
        for (int c = 0; c < 5; c++) {
            int x = 90 + c * 86, y = 187 + r * 72, v = wordle_game.result[r][c];
            rect(x, y, 76, 62, v == 2 ? 0 : v == 1 ? 175 : 255);
            key_outline(x, y, 76, 62);
            char k[2] = {wordle_game.guesses[r][c], 0};
            int old = ink_color;
            if (v == 2) {
                ink_color = -1; /* draw inverted glyph into dark tile via temporary white tile */
                rect(x + 2, y + 2, 72, 58, 255);
                centered(x, y + 2, 76, 58, k, ui_font(UI_TITLE));
                for (int yy = y + 2; yy < y + 60; yy++)
                    for (int xx = x + 2; xx < x + 74; xx++)
                        ui_pixels[yy * 600 + xx] = 255 - ui_pixels[yy * 600 + xx];
            } else
                centered(x, y + 2, 76, 58, k, ui_font(UI_TITLE));
            ink_color = old;
        }
    if (wordle_game.over) {
        snprintf(b, sizeof(b), "%s · %s", tr(wordle_game.over == 1 ? "Gewonnen!" : "Lösung"),
                 word_secret);
        centered(28, 652, 544, 42, b, ui_font(UI_PRIMARY));
        centered(28, 712, 544, 35, "Morgen ein neues Wort", ui_font(UI_SECONDARY));
    } else
        button_h(90, 663, 420, 60, "Wort eingeben", 0);
}
