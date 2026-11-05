/* App-owned module; assembled by apps/host/compose.h. */
static void app_snake(void) {
    heading("Snake");
    app_restart();
    char b[60];
    snprintf(b, sizeof(b), "%d  ·  %s %u", snake_game.score, tr("Rekord"), app_data.best_snake);
    drawtext(30, 132, b, ui_font(UI_BODY), 360);
    button_h(414, 124, 158, 40,
             snake_game.over     ? "Neu starten"
             : snake_game.paused ? "Start"
                                 : "Pause",
             0);
    int x0 = 30, y0 = 180, cell = 30;
    key_outline(x0 - 1, y0 - 1, 542, 482);
    if (snake_game.food >= 0)
        disc(x0 + (snake_game.food % 18) * cell + 15, y0 + (snake_game.food / 18) * cell + 15, 5,
             0);
    for (int i = 0; i < snake_game.length; i++) {
        int c = snake_game.cells[i];
        board_tile(x0 + (c % 18) * cell, y0 + (c / 18) * cell, cell, 0);
    }
    if (snake_game.length) {
        int head = snake_game.cells[0], hx = x0 + (head % 18) * cell + 15,
            hy = y0 + (head / 18) * cell + 15;
        disc(hx + snake_game.dx * 5 - snake_game.dy * 4, hy + snake_game.dy * 5 + snake_game.dx * 4,
             2, 255);
        disc(hx + snake_game.dx * 5 + snake_game.dy * 4, hy + snake_game.dy * 5 - snake_game.dx * 4,
             2, 255);
    }
    if (snake_game.paused || snake_game.over) {
        rect(166, 329, 268, 98, 255);
        centered(166, 337, 268, 44, snake_game.over ? "Spiel beendet" : "Bereit?", ui_font(UI_PRIMARY));
        centered(166, 383, 268, 36, "Wischen oder Pfeile", ui_font(UI_SECONDARY));
    }
    int x[] = {258, 153, 258, 363}, y[] = {677, 735, 735, 735},
        kind[] = {IC_UP, IC_PREV, IC_DOWN, IC_NEXT};
    for (int i = 0; i < 4; i++) {
        region(x[i], y[i], 84, 48);
        key_outline(x[i], y[i], 84, 48);
        icon(x[i] + 20, y[i] + 2, kind[i]);
    }
}
