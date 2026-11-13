#include "../games/package_ui.h"
#include "rules.h"

static unsigned char hex_board[121],hex_previous[121];
static int hex_turn=1,hex_win,hex_swap,hex_has_undo,dirty;
static void hex_center(int i, int *x, int *y) {
    int r = i / 11, c = i % 11;
    *x = 60 + r * 48;
    *y = 155 + c * 36 + r * 18;
}
static void draw(void) {
    ui_paint_heading("Hex",2);
    lumen_icon_action("help",468,74,IC_BOOK);lumen_icon_action("restart",528,74,IC_RESTART);
    const char *label=hex_win ?
        (hex_win==1 ? game_text("Schwarz gewinnt","Black wins","Noir gagne") : game_text("Grau gewinnt","Gray wins","Gris gagne")) :
        (hex_turn==1 ? game_text("Schwarz · links nach rechts","Black · left to right","Noir · gauche à droite") :
                       game_text("Grau · oben nach unten","Gray · top to bottom","Gris · haut en bas"));
    ui_paint_text(28,121,544,34,label,PAINT_BODY);
    for(int i=0;i<121;i++) {
        int x,y;hex_center(i,&x,&y);
        lumen_hex_cell(x,y,hex_board[i]==1 ? 0 : hex_board[i]==2 ? 161 : 255);
    }
    ui_paint_line(26,155,26,515,4,0);
    ui_paint_line(574,335,574,695,4,0);
    ui_paint_line(60,133,540,313,3,0);
    ui_paint_line(60,537,540,717,3,0);
    lumen_button_action("undo",28,735,260,48,game_text("Zug zurück","Undo move","Annuler"),0);
    lumen_button_action("secondary",312,735,260,48,hex_swap ? game_text("Seiten tauschen","Swap sides","Échanger") :
        game_text("Neue Partie","New game","Nouvelle partie"),0);
}
static int input(const Input *in) {
    char action[32];
    if(in->kind!=INPUT_TAP && !game_action(in,action,sizeof(action)))return 0;
    int x=in->x,y=in->y;
    if (in->kind==INPUT_COMPONENT && !strcmp(action,"restart")) {
        memset(hex_board, 0, 121);
        hex_turn = 1;
        hex_win = hex_swap = hex_has_undo = 0;
        dirty = 1;
    } else if(in->kind==INPUT_COMPONENT && !strcmp(action,"help")) {
        game_help("Hex",
          "Schwarz verbindet den linken mit dem rechten Rand, Grau den oberen mit dem unteren. Tippe ein leeres Feld. Grau darf nach dem ersten Zug die Seiten tauschen.",
          "Black connects left to right. Gray connects top to bottom. Tap an empty cell. Gray can swap sides after the first move.",
          "Noir relie la gauche à la droite. Gris relie le haut au bas. Touchez une case vide. Gris peut échanger les côtés après le premier coup.");
    } else if (in->kind==INPUT_COMPONENT) {
        if (!strcmp(action,"undo") && hex_has_undo) {
            memcpy(hex_board, hex_previous, 121);
            hex_turn = 3 - hex_turn;
            hex_win = 0;
            hex_has_undo = 0;
            dirty = 1;
        } else if (!strcmp(action,"secondary") && hex_swap && !hex_win) {
            for (int i = 0; i < 121; i++)
                if (hex_board[i])
                    hex_board[i] = 3 - hex_board[i];
            hex_swap = 0;
            hex_turn = 1;
            hex_has_undo = 0;
            dirty = 1;
        } else if (!strcmp(action,"secondary")) {
            memset(hex_board, 0, 121);
            hex_turn = 1;
            hex_win = hex_swap = hex_has_undo = 0;
            dirty = 1;
        }
    } else if (!hex_win) {
        int best = 1500, idx = -1;
        for (int i = 0; i < 121; i++) {
            int xx, yy;
            hex_center(i, &xx, &yy);
            int d = (xx - x) * (xx - x) + (yy - y) * (yy - y);
            if (abs(yy - y) <= 18 && abs(xx - x) <= 32 - abs(yy - y) * 16 / 18 && d < best) {
                best = d;
                idx = i;
            }
        }
        if (idx >= 0 && !hex_board[idx]) {
            memcpy(hex_previous, hex_board, 121);
            hex_has_undo = 1;
            hex_board[idx] = hex_turn;
            hex_win = hex_winner(hex_board, 11);
            int n = 0;
            for (int i = 0; i < 121; i++)
                n += hex_board[i] != 0;
            hex_swap = n == 1;
            hex_turn = 3 - hex_turn;
            dirty = 1;
        }
    }
    return 1;
}
static void save(void) {
    if(!dirty)return;
    unsigned char state[123];memcpy(state,hex_board,121);state[121]=hex_turn;state[122]=hex_swap;
    if(app_state_save("game.bin",state,sizeof(state)))dirty=0;
}
int main(void) {
    unsigned char state[123];
    int valid=app_state_load("game.bin",state,sizeof(state)) ||
              lumen_legacy_state("hex-1.bin",state,sizeof(state));
    if(valid) {
        for(int i=0;i<121;i++)if(state[i]>2)valid=0;
        if(valid) {
            memcpy(hex_board,state,121);hex_turn=state[121]==2 ? 2 : 1;
            hex_swap=state[122]!=0;hex_win=hex_winner(hex_board,11);dirty=1;
        }
    }
    lumen_run(draw,input,save);return 0;
}
