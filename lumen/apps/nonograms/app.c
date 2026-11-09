#include "../games/package_ui.h"
static unsigned char nono_solution[25],nono_cells[25];
static int nono_mark,dirty;
static unsigned rng;
static const char *title(void){return game_text("Nonogramme","Nonograms","Nonogrammes");}
static int nono_run_patterns(unsigned mask, unsigned *out) {
    unsigned n = 0, len = 0;
    for (int x = 0; x < 5; x++) {
        if (mask & (1u << x))
            len++;
        else if (len) {
            out[n++] = len;
            len = 0;
        }
    }
    if (len)
        out[n++] = len;
    return n;
}
static int nono_match(unsigned a, unsigned b) {
    unsigned x[5] = {0}, y[5] = {0};
    int n = nono_run_patterns(a, x), m = nono_run_patterns(b, y);
    return n == m && !memcmp(x, y, n * sizeof(unsigned));
}
static int nono_solve(int row, unsigned acc[5], unsigned target[5], int limit) {
    if (row == 5) {
        for (int c = 0; c < 5; c++) {
            unsigned a = 0, b = 0;
            for (int r = 0; r < 5; r++) {
                a |= ((acc[r] >> c) & 1) << r;
                b |= ((target[r] >> c) & 1) << r;
            }
            if (!nono_match(a, b))
                return 0;
        }
        return 1;
    }
    int count = 0;
    for (unsigned m = 0; m < 32; m++)
        if (nono_match(m, target[row])) {
            acc[row] = m;
            count += nono_solve(row + 1, acc, target, limit - count);
            if (count >= limit)
                return count;
        }
    return count;
}
static void v1_nono_new(void) {
    unsigned t[5], a[5];
    int found = 0;
    for (int tries = 0; tries < 200; tries++) {
        for (int r = 0; r < 5; r++)
            t[r] = (game_random(&rng) >> 12) & 31;
        if (nono_solve(0, a, t, 2) == 1) {
            found = 1;
            break;
        }
    }
    if (!found)
        for (int i = 0; i < 5; i++)
            t[i] = 31;
    for (int i = 0; i < 25; i++)
        nono_solution[i] = (t[i / 5] >> (i % 5)) & 1;
    memset(nono_cells, 0, 25);
    dirty=1;
}
static void draw(void) {
    ui_paint_heading(title(),2);
    ui_paint_icon_button(468,74,IC_BOOK);
    ui_paint_icon_button(528, 74, IC_RESTART);
    ui_paint_text(28, 133, 544, 32, game_text("5 × 5 · Ein Bild aus Logik","5 × 5 · Draw with logic","5 × 5 · Dessinez par logique"), PAINT_SECONDARY);
    for (int r = 0; r < 5; r++) {
        unsigned mask = 0, v[5];
        for (int c = 0; c < 5; c++)
            mask |= nono_solution[r * 5 + c] << c;
        int n = nono_run_patterns(mask, v);
        char b[64] = "";
        for (int i = 0; i < n; i++) {
            char x[8];
            snprintf(x, 8, "%u ", v[i]);
            strcat(b, x);
        }
        if (!n)
            strcpy(b, "0");
        ui_paint_text(18, 263 + r * 76, 126, 76, b, PAINT_HEADING);
    }
    for (int c = 0; c < 5; c++) {
        unsigned mask = 0, v[5];
        for (int r = 0; r < 5; r++)
            mask |= nono_solution[r * 5 + c] << r;
        int n = nono_run_patterns(mask, v);
        for (int j = 0; j < n; j++) {
            char b[8];
            snprintf(b, 8, "%u", v[j]);
            ui_paint_text(146 + c * 76, 243 - (n - j) * 30, 76, 30, b, PAINT_BODY);
        }
    }
    int win = 1;
    for (int i = 0; i < 25; i++) {
        int x = 146 + (i % 5) * 76, y = 263 + (i / 5) * 76;
        ui_paint_hitbox(x,y,76,76);
        ui_paint_surface(x,y,76,76,5,nono_cells[i]==1 ? 0 : 255,1);
        if (nono_cells[i] == 2) {
            ui_paint_line(x + 24, y + 24, x + 52, y + 52, 2,0);
            ui_paint_line(x + 52, y + 24, x + 24, y + 52, 2,0);
        }
        if ((nono_cells[i] == 1) != nono_solution[i])
            win = 0;
    }
    lumen_button(28,692,262,48,game_text("Füllen","Fill","Remplir"),!nono_mark);
    lumen_button(310,692,262,48,game_text("Leer markieren","Mark empty","Marquer vide"),nono_mark);
    if (win)
        ui_paint_text(28, 746, 544, 40, game_text("Gelöst!","Solved!","Résolu !"), PAINT_HEADING);
}

static int input(const Input *in) {
    if(in->kind!=INPUT_TAP)return 1;
    int x=in->x,y=in->y;
    if(y>=74 && y<118) {
        if(x>=528)v1_nono_new();
        else if(x>=468 && x<512)game_help(title(),
            "Die Zahlen geben die Länge zusammenhängender Blöcke an. Zwischen zwei Blöcken muss mindestens ein leeres Feld liegen.",
            "Clues give the lengths of consecutive runs. Leave at least one empty square between runs, and mark empty squares as you go.",
            "Les indices donnent la longueur des suites. Laissez au moins une case vide entre deux suites.");
    } else if(y>=692 && y<740){nono_mark=x>=300;dirty=1;}
    else if(x>=146 && x<526 && y>=263 && y<643) {
        int i=(y-263)/76*5+(x-146)/76,val=nono_mark ? 2 : 1;
        nono_cells[i]=nono_cells[i]==val ? 0 : val;dirty=1;
    }
    return 1;
}
static void save(void) {
    if(!dirty)return;
    unsigned char state[51];
    memcpy(state,nono_solution,25);memcpy(state+25,nono_cells,25);state[50]=nono_mark;
    if(app_state_save("game.bin",state,sizeof(state)))dirty=0;
}
int main(void) {
    rng=(unsigned)time(NULL)^(unsigned)ui_clock_millis();
    unsigned char state[51];
    int valid=app_state_load("game.bin",state,sizeof(state)) ||
              lumen_legacy_state("nonogram-1.bin",state,sizeof(state));
    if(valid) {
        valid=state[50]<2;
        for(int i=0;i<25;i++)if(state[i]>1 || state[i+25]>2)valid=0;
    }
    if(valid){memcpy(nono_solution,state,25);memcpy(nono_cells,state+25,25);nono_mark=state[50];dirty=1;}
    else v1_nono_new();
    lumen_run(draw,input,save);return 0;
}
