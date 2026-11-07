/* App-owned module; assembled by apps/host/compose.h. */
static void app_mines(void) {
    heading("Minesweeper");
    app_restart();
    char b[80];
    snprintf(b, sizeof(b), "%d / 12", mine_game.flags);
    drawtext(30, 129, b, ui_font(UI_BODY), 300);
    button_h(414, 122, 158, 36, mine_flags ? "Aufdecken" : "Fahne", mine_flags);
    outline_surface(28,168,544,584,RADIUS_SMALL,104);
    for (int i = 0; i < 90; i++) {
        int x = 30 + (i % 9) * 60, y = 170 + (i / 9) * 58;
        if (!mine_game.open[i]) {
            rect(x, y, 58, 56, 215);
            if (!mine_game.over)
                region(x, y, 58, 56);
        } else {
            rect(x, y, 58, 56, 255);

        }
        rect(x+58,y,2,58,174);
        rect(x,y+56,60,2,174);
        if (mine_game.flag[i])
            icon(x + 3, y + 2, IC_FLAG);
        else if (mine_game.bomb[i] && mine_game.over) {
            for (int k=0;k<8;k++) {
                double a=k*3.141592653589793/4;
                stroke(x+29+cos(a)*8,y+28+sin(a)*8,
                       x+29+cos(a)*16,y+28+sin(a)*16,3);
            }
            aa_disc(x+29,y+28,10,0);
            aa_disc(x+26,y+24,3,255);
        } else if (mine_game.open[i]) {
            int n = mines_adjacent(&mine_game, i);
            if (n) {
                snprintf(b, sizeof(b), "%d", n);
                centered(x, y, 58, 56, b, ui_font(UI_BODY));
            }
        }
    }
    centered(28, 755, 544, 42,
             mine_game.over ? (mine_game.won ? "Gewonnen!" : "Leider eine Mine.")
                            : "Tippen: aufdecken · Halten: markieren",
             ui_font(UI_SECONDARY));
}
