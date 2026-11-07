/* App-owned module; assembled by apps/host/compose.h. */
static void app_hangman(void) {
    heading("Wortraten");
    app_restart();
    stroke(118, 320, 300, 320, 2);
    stroke(154, 320, 154, 151, 2);
    stroke(154, 151, 260, 151, 2);
    stroke(260, 151, 260, 175, 2);
    int n = word_game.misses;
    if (n > 0)
        ring(260, 193, 17, 2);
    if (n > 1)
        stroke(260, 210, 260, 264, 2);
    if (n > 2)
        stroke(260, 224, 230, 245, 2);
    if (n > 3)
        stroke(260, 224, 290, 245, 2);
    if (n > 4)
        stroke(260, 264, 235, 300, 2);
    if (n > 5)
        stroke(260, 264, 285, 300, 2);
    int len = (int)strlen(word_game.word), w = 34, x = (600 - w * len) / 2;
    for (int i = 0; i < len; i++) {
        char ch[2] = {word_game.word[i], 0};
        if (word_game.over || (word_game.guesses & (1u << (ch[0] - 'A'))))
            centered(x + i * w, 365, w, 44, ch, ui_font(UI_PRIMARY));
        rect(x + i * w + 4, 414, w - 8, 2, 0);
    }
    for (int i = 0; i < 26; i++) {
        int row = i / 10, col = i % 10, cols = row == 2 ? 6 : 10,
            xx = (600 - cols * 54) / 2 + col * 54, y = 455 + row * 58;
        char key[2] = {(char)('A' + i), 0};
        if (word_game.guesses & (1u << i)) {
            control_surface(xx, y, 48, 48, RADIUS_MEDIUM, 220);
            centered(xx, y, 48, 48, key, ui_font(UI_BODY));
        } else
            button_h(xx, y, 48, 48, key, 0);
    }
    if (word_game.over && !word_game.won) {
        char solution[80];
        snprintf(solution, sizeof(solution), "%s: %s", tr("Das Wort war"), word_game.word);
        centered(28, 626, 544, 40, solution, ui_font(UI_PRIMARY));
    }
    centered(28, 690, 544, 48,
             word_game.over
                 ? (word_game.won ? "Gewonnen!" : "Neues Spiel über das Neustart-Symbol.")
             : options[OPT_LANGUAGE] == 3 ? "Ein Wort, sechs Versuche (Pinyin)"
                                          : "Ein Wort, sechs Versuche",
             ui_font(UI_BODY));
}
