#include "../games/package_ui.h"
#include "../games/puzzles.h"

static Patience solitaire;
static int pat_source=-99,pat_index,dirty;
static void paint_disc(int x,int y,int radius,int gray){ui_paint_ellipse(x,y,radius,radius,gray);}
static void paint_stroke(int x,int y,int xx,int yy,int weight){ui_paint_line(x,y,xx,yy,weight,0);}
static void suit_icon(int x, int y, int suit) {
    if (suit == 0) {
        paint_disc(x - 4, y - 3, 4, 0);
        paint_disc(x + 4, y - 3, 4, 0);
        paint_stroke(x - 7, y - 2, x, y + 7, 5);
        paint_stroke(x + 7, y - 2, x, y + 7, 5);
    } else if (suit == 1) {
        paint_stroke(x, y - 8, x + 6, y, 3);
        paint_stroke(x + 6, y, x, y + 8, 3);
        paint_stroke(x, y + 8, x - 6, y, 3);
        paint_stroke(x - 6, y, x, y - 8, 3);
    } else if (suit == 2) {
        paint_disc(x, y - 4, 4, 0);
        paint_disc(x - 4, y + 2, 4, 0);
        paint_disc(x + 4, y + 2, 4, 0);
        paint_stroke(x, y + 3, x, y + 9, 3);
    } else {
        paint_stroke(x, y - 8, x - 5, y, 5);
        paint_stroke(x, y - 8, x + 5, y, 5);
        paint_disc(x - 3, y + 1, 4, 0);
        paint_disc(x + 3, y + 1, 4, 0);
        paint_stroke(x, y + 2, x, y + 9, 3);
    }
}
static void card(int x, int y, int id, int hidden, int selected) {
    ui_paint_surface(x,y,72,96,7,255,selected ? 3 : 1);
    if (hidden) {
        ui_paint_surface(x+6,y+6,60,84,4,222,1);
        for(int i=0;i<6;i++) {
            ui_paint_line(x+10,y+20+i*11,x+62,y+12+i*11,1,127);
            ui_paint_line(x+10,y+12+i*11,x+62,y+20+i*11,1,155);
        }
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
    lumen_text(x+6,y+4,40,30,b,PAINT_BODY,suit<2 ? 78 : 0,1);
    suit_icon(x + 54, y + 17, suit);
    suit_icon(x + 36, y + 58, suit);

}
static int column_gap(int c) {
    int n = solitaire.state.count[c];
    return n > 1 ? ((350 / (n - 1)) < 27 ? 350 / (n - 1) : 27) : 27;
}
static void draw(void) {
    ui_paint_heading("Solitaire", 4);
    ui_paint_icon_button(348,74,IC_BOOK);
    ui_paint_icon_button(408, 74, IC_UNDO);
    ui_paint_icon_button(468, 74, IC_DONE);
    ui_paint_icon_button(528, 74, IC_RESTART);
    PatienceState *s = &solitaire.state;
    ui_paint_hitbox(21, 132, 72, 96);
    card(21, 132, -1, s->ns > 0, 0);
    if (!s->ns)
        ui_paint_icon_button(35,158,IC_RESTART);
    ui_paint_hitbox(101, 132, 72, 96);
    card(101, 132, s->nw ? s->waste[s->nw - 1] : -1, 0, pat_source == -1);
    for (int i = 0; i < 4; i++) {
        ui_paint_hitbox(261 + i * 80, 132, 72, 96);
        card(261 + i * 80, 132, s->foundation[i] ? i * 13 + s->foundation[i] - 1 : -1, 0,
             pat_source == 7 + i);
        if (!s->foundation[i])
            suit_icon(297 + i * 80, 180, i);
    }
    for (int c = 0; c < 7; c++) {
        int x = 21 + c * 80, gap = column_gap(c);
        if (!s->count[c]) {
            ui_paint_surface(x,248,72,96,7,255,1);
            ui_paint_hitbox(x, 248, 72, 96);
        }
        for (int i = 0; i < s->count[c]; i++) {
            int y = 248 + i * gap;
            card(x, y, s->column[c][i], i < s->hidden[c], pat_source == c && pat_index == i);
            ui_paint_hitbox(x, y, 72, i == s->count[c] - 1 ? 96 : gap);
        }
    }
    char b[100];
    snprintf(b, sizeof(b), "%s %u · %s %d", game_text("Züge","Moves","Coups"), s->moves, game_text("Stapel","Stock","Pioche"), s->ns);
    lumen_text(28,728,544,38,b,PAINT_SECONDARY,100,1);
    if(patience_won(&solitaire))ui_paint_text(28,681,544,45,
        game_text("Geschafft!","Solved!","Réussi !"),PAINT_HEADING);
}
static int input(const Input *in) {
    if(in->kind!=INPUT_TAP)return 1;
    int x=in->x,y=in->y;
    dirty=1;
    if (y >= 74 && y < 118) {
        if (x >= 528)
            patience_new(&solitaire,(uint32_t)ui_clock_millis());
        else if (x >= 468 && x<512)
            patience_auto(&solitaire);
        else if (x >= 408 && x<452)
            patience_undo(&solitaire);
        else if (x>=348 && x<392)game_help("Solitaire",
            "Lege die Karten nach Farben von Ass bis König ab. Im Tableau wechselst du Rot und Schwarz in absteigender Reihenfolge. Leere Spalten nehmen Könige auf.",
            "Build each suit from ace to king. In the tableau, build downward and alternate red and black. Only kings go in empty columns.",
            "Empilez chaque couleur de l’as au roi. Sur le tableau, descendez en alternant rouge et noir. Seuls les rois occupent les colonnes vides.");
        pat_source = -99;
    } else if (y >= 132 && y < 228) {
        if (x >= 21 && x < 93) {
            patience_draw(&solitaire);
            pat_source = -99;
        } else if (x >= 101 && x < 173 && solitaire.state.nw) {
            pat_source = -1;
            pat_index = 0;
        } else if (x >= 261 && x < 573) {
            int to = 7 + (x - 261) / 80;
            if (pat_source != -99 && patience_move(&solitaire, pat_source, pat_index, to))
                pat_source = -99;
            else if (solitaire.state.foundation[to - 7]) {
                pat_source = to;
                pat_index = 0;
            }
        }
    } else if (y >= 248 && y < 710 && x >= 21 && x < 573) {
        int c = (x - 21) / 80;
        if (c < 7) {
            if (pat_source != -99 && patience_move(&solitaire, pat_source, pat_index, c))
                pat_source = -99;
            else if (solitaire.state.count[c]) {
                int idx = (y - 248) / column_gap(c);
                if (idx >= solitaire.state.count[c])
                    idx = solitaire.state.count[c] - 1;
                if (idx >= solitaire.state.hidden[c]) {
                    pat_source = c;
                    pat_index = idx;
                }
            }
        }
    }
    return 1;
}
static int valid_patience(void) {
    PatienceState *s = &solitaire.state;
    if (s->ns > 24 || s->nw > 24 || s->ns + s->nw > 24 || solitaire.undos < 0 ||
        solitaire.undos > 24)
        return 0;
    for (int i = 0; i < 7; i++) {
        if (s->count[i] > 52 || s->hidden[i] > s->count[i])
            return 0;
        for (int j = 0; j < s->count[i]; j++)
            if (s->column[i][j] >= 52)
                return 0;
    }
    for (int i = 0; i < 4; i++)
        if (s->foundation[i] > 13)
            return 0;
    return 1;
}
static void save(void){if(dirty && app_state_save("game.bin",&solitaire,sizeof(solitaire)))dirty=0;}
int main(void) {
    if(!(app_state_load("game.bin",&solitaire,sizeof(solitaire)) ||
         lumen_legacy_state("solitaire-v1.bin",&solitaire,sizeof(solitaire))) || !valid_patience())
        patience_new(&solitaire,(uint32_t)ui_clock_millis());
    dirty=1;lumen_run(draw,input,save);return 0;
}
