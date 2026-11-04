/* App-owned module; assembled by apps/host/compose.h. */
static void board_tile(int x, int y, int size, int color) {
    rect(x + 1, y + 1, size - 2, size - 2, color);
}
static void app_blocks(void) {
    heading("Tetris");
    app_restart();
    button_h(414, 124, 158, 40,
             block_game.over     ? "Neu starten"
             : block_game.paused ? "Start"
                                 : "Pause",
             0);
    int x0 = 28, y0 = 132, cell = 36;
    rect(x0 - 1, y0 - 1, 362, 578, 170);
    rect(x0, y0, 360, 576, 255);
    for (int y = 0; y < 16; y++)
        for (int x = 0; x < 10; x++)
            if (block_game.board[y][x])
                board_tile(x0 + x * cell, y0 + y * cell, cell, 65);
    if (!block_game.over) {
        int ghost = blocks_landing_y(&block_game);
        for (int y = 0; y < 4; y++)
            for (int x = 0; x < 4; x++)
                if (blocks_cell(block_game.piece, block_game.rotation, x, y)) {
                    int xx = x0 + (block_game.x + x) * cell, yy = y0 + (ghost + y) * cell;
                    if (ghost != block_game.y)
                        key_outline(xx + 2, yy + 2, cell - 4, cell - 4);
                    board_tile(xx, y0 + (block_game.y + y) * cell, cell, 0);
                }
    }
    char b[40];
    drawtext(410, 211, "Punkte", ui_font(UI_SECONDARY), 162);
    snprintf(b, sizeof(b), "%d", block_game.score);
    drawtext(410, 246, b, ui_font(UI_PRIMARY), 162);
    drawtext(410, 309, "Linien", ui_font(UI_SECONDARY), 162);
    snprintf(b, sizeof(b), "%d", block_game.lines);
    drawtext(410, 344, b, ui_font(UI_PRIMARY), 162);
    drawtext(410, 407, "Als Nächstes", ui_font(UI_SECONDARY), 162);
    for (int y = 0; y < 4; y++)
        for (int x = 0; x < 4; x++)
            if (blocks_cell(block_game.next, 0, x, y))
                board_tile(408 + x * 22, 452 + y * 22, 22, 0);
    if (block_game.paused || block_game.over) {
        rect(75, 334, 238, 100, 255);
        centered(75, 345, 238, 50, block_game.over ? "Spiel beendet" : "Bereit?", ui_font(UI_PRIMARY));
        centered(75, 395, 238, 30, "Start oben antippen", ui_font(UI_SECONDARY));
    }
    int kinds[] = {IC_PREV, IC_ROTATE, IC_NEXT, IC_DOWN};
    for (int i = 0; i < 4; i++) {
        int x = 49 + i * 136;
        region(x, 735, 94, 48);
        key_outline(x, 735, 94, 48);
        icon(x + 25, 737, kinds[i]);
    }
}
