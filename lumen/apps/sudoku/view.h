/* App-owned module; assembled by apps/host/compose.h. */
static void app_sudoku(void) {
    heading_actions("Sudoku", 1);
    icon_button(528, 74, IC_RESTART);
    button_h(28, 120, 140, 36, "Notizen", sudoku_pencil);
    icon_button(184, 116, IC_UNDO);
    icon_button(240, 116, IC_DELETE);
    button_h(360, 120, 150, 36, "Hinweis", 0);
    for (int i = 0; i < 81; i++) {
        int x = 39 + i % 9 * 58, y = 170 + i / 9 * 58;
        if (i == sudoku_selected)
            rect(x + 1, y + 1, 56, 56, 225);
        else if (sudoku_selected >= 0 && sudoku.board.cell[sudoku_selected] &&
                 sudoku.board.cell[i] == sudoku.board.cell[sudoku_selected])
            rect(x + 1, y + 1, 56, 56, 242);
        region(x, y, 58, 58);
        char b[8];
        if (sudoku.board.cell[i]) {
            snprintf(b, sizeof(b), "%d", sudoku.board.cell[i]);
            ink_color = sudoku.given[i] ? 0 : 70;
            centered(x, y, 58, 58, b, ui_font(UI_PRIMARY));
            if (sudoku.given[i])
                centered(x + 1, y, 58, 58, b, ui_font(UI_PRIMARY));
            ink_color = 0;
            if (sudoku_conflict(&sudoku, i))
                rect(x + 15, y + 48, 28, 3, 0);
        } else
            for (int k = 0; k < 9; k++)
                if (sudoku.board.notes[i] & (1 << k)) {
                    snprintf(b, sizeof(b), "%d", k + 1);
                    centered(x + k % 3 * 19, y + k / 3 * 19, 20, 20, b, ui_font(UI_SECONDARY));
                }
    }
    for (int i = 0; i <= 9; i++) {
        rect(39 + i * 58, 170, i % 3 ? 1 : 3, 522, i % 3 ? 185 : 50);
        rect(39, 170 + i * 58, 522, i % 3 ? 1 : 3, i % 3 ? 185 : 50);
    }
    for (int v = 1; v <= 9; v++) {
        char b[8];
        snprintf(b, sizeof(b), "%d", v);
        button_h(28 + (v - 1) * 61, 725, 56, 52, b, 0);
    }
    if (sudoku_won(&sudoku))
        centered(28, 692, 544, 28, "Geschafft!", ui_font(UI_BODY));
}
