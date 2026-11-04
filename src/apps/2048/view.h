/* App-owned module; assembled by apps/host/compose.h. */
static void app_tiles(void) {
    heading_actions("2048", 3);
    icon_button(468, 74, IC_UNDO);
    icon_button(528, 74, IC_RESTART);
    char b[100];
    snprintf(b, sizeof(b), "%s  %llu", tr("Punkte"), (unsigned long long)tile_game.state.score);
    drawtext_raw(30, 130, b, ui_font(UI_PRIMARY), 540);
    for (int i = 0; i < 16; i++) {
        int x = 56 + i % 4 * 124, y = 184 + i / 4 * 124, v = tile_game.state.cell[i];
        int colors[] = {250, 240, 200, 184, 168, 152, 136, 120, 104, 88, 72, 56};
        int color = colors[v < 12 ? v : 11];
        if (color < 28)
            color = 28;
        rect(x, y, 116, 116, color);
        if (v) {
            snprintf(b, sizeof(b), "%u", 1u << v);
            ink_color = color < 140 ? 255 : 0;
            centered(x, y, 116, 116, b, v >= 20 ? ui_font(UI_BODY) : v >= 10 ? ui_font(UI_TITLE) : ui_font(UI_DISPLAY));
            ink_color = 0;
        }
    }
    const int kinds[] = {IC_PREV, IC_UP, IC_DOWN, IC_NEXT};
    for (int i = 0; i < 4; i++) {
        button_h(28 + i * 140, 716, 124, 56, "", 0);
        icon(68 + i * 140, 722, kinds[i]);
    }
    if (tiles_over(&tile_game) || tile_game.state.won)
        drawtext(30, 682,
                 tiles_over(&tile_game) ? "Keine Züge mehr. Undo oder neu starten."
                                        : "2048 erreicht! Du kannst weiterspielen.",
                 ui_font(UI_SECONDARY), 540);
}
