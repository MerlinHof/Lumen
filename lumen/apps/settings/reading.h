static const char *progress15[] = {"Default",  "Seitenpfeile",  "Prozent",           "Seitenzahl",
                                   "Restzeit", "Keine Anzeige", "Fortschrittsbalken"};
/* App-owned module; assembled by apps/host/compose.h. */
static const char *reader_families[] = {"Charis", "Sans Serif", "Monospace", "Times", "Noto Serif"};
static const char *reader_align[] = {"Linksbündig", "Blocksatz"};
static const char *reader_zones[] = {"Links / rechts", "Mehr Platz vorwärts"};
static const char *reader_labels[] = {
    "Schriftfamilie", "Schriftgröße",   "Zeilenabstand",        "Seitenrand", "Ausrichtung",
    "Absatzabstand",  "Zeichenabstand", "Wischen zum Blättern", "Tippzonen",  "Lesefortschritt"};
static void settings_screen(void) {
    heading("Lesen");
    drawtext(30, 121, reading_page ? "Abstände & Gesten" : "Schrift & Satz", ui_font(UI_SECONDARY), 540);
    for (int row = 0; row < 6 && reading_page * 6 + row < 10; row++) {
        int id = reading_page * 6 + row, y = 155 + row * 84;
        form_label(30, y, 310, 44, reader_labels[id]);
        if (id == 0 || id == 4 || id == 8) {
            const char *value = id == 0   ? reader_families[options[OPT_FONT] % 5]
                                : id == 4 ? reader_align[options[OPT_ALIGN] % 2]
                                          : reader_zones[options[OPT_TAP] % 2];
            dropdown_button(350, y, 222, 44, value);
        } else if (id == 9)
            dropdown_button(350, y, 222, 44, progress15[options[OPT_PROGRESS] ? reader_progress_mode : 5]);
        else if (id == 7)
            toggle(492, y + 6, options[OPT_SWIPE]);
        else {
            char value[32];
            int v = id == 1   ? settings.size
                    : id == 2 ? settings.leading
                    : id == 3 ? settings.margin
                              : options[id == 5 ? OPT_PARAGRAPH : OPT_LETTERSPACE];
            if (id == 2)
                snprintf(value, sizeof(value), "%.2f", v / 100.0);
            else
                snprintf(value, sizeof(value), "%d", v);
            button_h(394, y, 48, 44, "-", 0);
            centered(444, y, 78, 44, value, ui_font(UI_BODY));
            button_h(524, y, 48, 44, "+", 0);
        }
        divider(28, y + 66, 544);
    }
    pager(679, &reading_page, 2);
}
static void reading_input(const Input *in) {
    if (in->kind != INPUT_TAP)
        return;
    int x = in->x, y = in->y;
    if (y >= 679 && y < 723) {
        reading_page = x >= 300 ? 1 : 0;
        return;
    }
    if (y < 155 || y >= 659)
        return;
    int row = (y - 155) / 84, id = reading_page * 6 + row, top = 155 + row * 84;
    if (id >= 10 || y >= top + 44)
        return;
    if (id == 0 || id == 4 || id == 8) {
        if (x < 350)
            return;
        int opt = id == 0 ? OPT_FONT : id == 4 ? OPT_ALIGN : OPT_TAP;
        const char **labels = id == 0 ? reader_families : id == 4 ? reader_align : reader_zones;
        open_dropdown(100 + opt, labels, id == 0 ? 5 : 2, options[opt], 220, top + 50, 350);
        return;
    }
    if (id == 9) {
        if (x >= 350)
            open_dropdown(916, progress15, 7, options[OPT_PROGRESS] ? reader_progress_mode : 5,
                          220, top + 50, 350);
        return;
    }
    if (id == 7 && x < 484)
        return;
    if (id == 7)
        options[OPT_SWIPE] ^= 1;
    else {
        int direction = x >= 524 ? 1 : x >= 394 && x < 442 ? -1 : 0;
        int *value = id == 1   ? &settings.size
                     : id == 2 ? &settings.leading
                     : id == 3 ? &settings.margin
                               : &options[id == 5 ? OPT_PARAGRAPH : OPT_LETTERSPACE];
        int step = id == 1   ? 2
                   : id == 2 ? 5
                   : id == 3 ? 3
                   : id == 5 ? 4
                             : 2,
            min = id == 1   ? 16
                  : id == 2 ? 110
                            : 0,
            max = id == 1   ? 34
                  : id == 2 ? 190
                  : id == 3 ? 30
                  : id == 5 ? 24
                            : 20;
        *value += step * direction;
        if (*value < min)
            *value = min;
        if (*value > max)
            *value = max;
    }
    library_settings_save();
}
