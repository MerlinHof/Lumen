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
    if (snake_game.food >= 0) {
        int fx=x0+(snake_game.food%18)*cell+15, fy=y0+(snake_game.food/18)*cell+15;
        ui_paint_ellipse(fx-4,fy+1,7,9,16);
        ui_paint_ellipse(fx+4,fy+1,7,9,16);
        stroke(fx,fy-8,fx+1,fy-14,2);
        ui_paint_ellipse(fx+5,fy-12,5,2,104);
        stroke(fx-5,fy-3,fx-6,fy+2,2);
    }
    for (int pass=0;pass<2;pass++) {
        for (int i=snake_game.length-1;i>0;i--) {
            int at=snake_game.cells[i], next=snake_game.cells[i-1];
            int cx=x0+(at%18)*cell+15,cy=y0+(at/18)*cell+15;
            int nx=x0+(next%18)*cell+15,ny=y0+(next/18)*cell+15;
            stroke_color(cx,cy,nx,ny,pass ? 16 : 22,pass ? 112 : 24);
        }
    }
    if (snake_game.length) {
        int at=snake_game.cells[0],cx=x0+(at%18)*cell+15,cy=y0+(at/18)*cell+15;
        int dx=snake_game.dx,dy=snake_game.dy;
        aa_disc(cx,cy,11,24);
        aa_disc(cx,cy,8,112);
        for (int side=-1;side<=1;side+=2) {
            aa_disc(cx+dx*4-dy*side*5,cy+dy*4+dx*side*5,3,255);
            aa_disc(cx+dx*5-dy*side*5,cy+dy*5+dx*side*5,1,0);
        }
    }
    if (snake_game.paused || snake_game.over) {
        control_surface(166, 229, 268, 98, RADIUS_MEDIUM, 255);
        centered(166, 237, 268, 44, snake_game.over ? "Spiel beendet" : "Bereit?", ui_font(UI_PRIMARY));
        centered(166, 283, 268, 36, "Wischen oder Pfeile", ui_font(UI_SECONDARY));
    }
    int x[] = {258, 153, 258, 363}, y[] = {677, 735, 735, 735},
        kind[] = {IC_UP, IC_PREV, IC_DOWN, IC_NEXT};
    for (int i = 0; i < 4; i++) {
        region(x[i], y[i], 84, 48);
        key_outline(x[i], y[i], 84, 48);
        icon(x[i] + 20, y[i] + 2, kind[i]);
    }
}
