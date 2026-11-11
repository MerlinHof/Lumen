#include "lumen_app.h"
#include <math.h>

static const char *local(const char *de,const char *en,const char *fr) {
    return lumen_language==1 ? en : lumen_language==2 ? fr : de;
}
static void help(const char *name,const char *text) {
    lumen_app_open_with("lumen.reader","open_document","",name,text);
}

typedef struct {
    unsigned char bomb[90], open[90], flag[90];
    int started, over, won, flags;
} Mines;
static Mines mine_game;
static unsigned random_below(unsigned limit){return limit ? (unsigned)rand()%limit : 0;}
static void mines_new(Mines *m);
static int mines_adjacent(const Mines *m, int c);
static void mines_flag(Mines *m, int c);
static void mines_reveal(Mines *m, int c);
static void mines_new(Mines *m) {
    memset(m, 0, sizeof(*m));
}
static int mines_adjacent(const Mines *m, int c) {
    int n = 0, x = c % 9, y = c / 9;
    for (int dy = -1; dy <= 1; dy++)
        for (int dx = -1; dx <= 1; dx++)
            if ((dx || dy) && x + dx >= 0 && x + dx < 9 && y + dy >= 0 && y + dy < 10)
                n += m->bomb[(y + dy) * 9 + x + dx];
    return n;
}
static void mines_flag(Mines *m, int c) {
    if (c < 0 || c >= 90 || m->over || m->open[c])
        return;
    m->flag[c] ^= 1;
    m->flags += m->flag[c] ? 1 : -1;
}
static void mines_reveal(Mines *m, int c) {
    if (c < 0 || c >= 90 || m->over || m->flag[c])
        return;
    if (!m->started) {
        for (int n = 0; n < 12;) {
            int k = (int)random_below(90);
            if (abs(k % 9 - c % 9) <= 1 && abs(k / 9 - c / 9) <= 1)
                continue;
            if (!m->bomb[k]) {
                m->bomb[k] = 1;
                n++;
            }
        }
        m->started = 1;
    }
    if (m->open[c])
        return;
    if (m->bomb[c]) {
        m->open[c] = 1;
        m->over = 1;
        return;
    }
    int queue[90], head = 0, tail = 0;
    queue[tail++] = c;
    m->open[c] = 1;
    while (head < tail) {
        int k = queue[head++];
        if (mines_adjacent(m, k))
            continue;
        int x = k % 9, y = k / 9;
        for (int dy = -1; dy <= 1; dy++)
            for (int dx = -1; dx <= 1; dx++)
                if (x + dx >= 0 && x + dx < 9 && y + dy >= 0 && y + dy < 10) {
                    int j = (y + dy) * 9 + x + dx;
                    if (!m->open[j] && !m->flag[j] && !m->bomb[j]) {
                        m->open[j] = 1;
                        queue[tail++] = j;
                    }
                }
    }
    int open = 0;
    for (int i = 0; i < 90; i++)
        open += m->open[i];
    if (open == 78)
        m->won = m->over = 1;
}

static int flag_mode,held_cell=-1;
static void restart(void){mines_new(&mine_game);flag_mode=0;held_cell=-1;}
static int cell_at(const Input *in) {
    if(in->x<30 || in->x>=570 || in->y<170 || in->y>=750)return -1;
    return (in->y-170)/58*9+(in->x-30)/60;
}
static int input(const Input *in) {
    int cell=cell_at(in);
    if(in->kind==INPUT_HOLD && cell>=0){mines_flag(&mine_game,cell);held_cell=cell;return 1;}
    if(in->kind==INPUT_SWIPE){held_cell=-1;return 1;}
    if(in->kind!=INPUT_TAP)return 0;
    if(held_cell>=0){int held=held_cell;held_cell=-1;if(cell==held)return 1;}
    if(in->y>=74 && in->y<118) {
        if(in->x>=528)restart();
        else if(in->x>=468)help(local("Minesweeper","Minesweeper","Démineur"),
            local("Decke alle sicheren Felder auf. Zahlen zählen die Minen auf den acht Nachbarfeldern. Halte ein Feld gedrückt, um es zu markieren.",
            "Reveal every safe square. Numbers count mines in the eight neighboring squares. Hold a square to flag it.",
            "Révélez toutes les cases sûres. Les chiffres comptent les mines voisines. Maintenez une case pour la marquer."));
    } else if(in->y>=122 && in->y<158 && in->x>=414)flag_mode^=1;
    else if(cell>=0) {
        if(flag_mode)mines_flag(&mine_game,cell);
        else mines_reveal(&mine_game,cell);
    }
    return 1;
}
/* Adapters to shared SDK drawing primitives. */
#define RADIUS_SMALL 6
#define UI_BODY PAINT_BODY
#define UI_SECONDARY PAINT_SECONDARY
#define ui_font(x) (x)
#define centered(x,y,w,h,s,r) ui_paint_text(x,y,w,h,s,r)
#define drawtext(x,y,s,r,w) lumen_text(x,y,w,34,s,r,-1,1)
#define region ui_paint_hitbox
#define rect(x,y,w,h,c) ui_paint_surface(x,y,w,h,0,c,c)
#define stroke(x,y,xx,yy,w) ui_paint_line(x,y,xx,yy,w,0)
#define aa_disc(x,y,r,c) ui_paint_ellipse(x,y,r,r,c)
#define outline_surface(x,y,w,h,r,c) ui_paint_surface(x,y,w,h,r,255,c)
#define button_h lumen_button
#define icon ui_paint_icon_button
static void draw(void) {
    ui_paint_heading(local("Minesweeper","Minesweeper","Démineur"),2);
    lumen_button(468,74,44,44,"?",0);
    ui_paint_icon_button(528,74,IC_RESTART);
    char b[80];
    snprintf(b, sizeof(b), "%d / 12", mine_game.flags);
    drawtext(30, 129, b, ui_font(UI_BODY), 300);
    button_h(414, 122, 158, 36, flag_mode ? local("Aufdecken","Reveal","Révéler") : local("Fahne","Flag","Drapeau"), flag_mode);
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
             mine_game.over ? (mine_game.won ? local("Gewonnen!","You won!","Gagné !") : local("Leider eine Mine.","You hit a mine.","Vous avez touché une mine."))
                            : local("Tippen: aufdecken · Halten: markieren","Tap to reveal · Hold to flag","Toucher pour révéler · Maintenir pour marquer"),
             ui_font(UI_SECONDARY));
}

int main(void) {
    srand((unsigned)time(NULL));
    restart();
    lumen_run(draw,input,NULL);
    return 0;
}
