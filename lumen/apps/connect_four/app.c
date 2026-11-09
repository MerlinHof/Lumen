#include "../games/package_ui.h"

enum { COLS=7, ROWS=6 };
static struct { unsigned char board[ROWS][COLS];int moves,turn,winner; } game={.turn=1};
static int dirty;
static const char *title(void){return game_text("Vier gewinnt","Connect Four","Puissance 4");}
static void draw(void) {
    ui_paint_heading(title(),2);
    ui_paint_icon_button(468,74,IC_BOOK);
    ui_paint_icon_button(528,74,IC_RESTART);
    char label[80];
    snprintf(label,sizeof(label),game.winner==3 ? "%s" : "%s %d",
        game.winner==3 ? game_text("Unentschieden","Draw","Match nul") :
        game.winner ? game_text("Gewonnen · Spieler","Winner · Player","Gagnant · Joueur") :
                      game_text("Am Zug · Spieler","To play · Player","Au tour du joueur"),
        game.winner ? game.winner : game.turn);
    ui_paint_text(28,137,544,46,label,PAINT_HEADING);
    for(int row=0;row<ROWS;row++)for(int col=0;col<COLS;col++) {
        int x=40+col*75,y=210+row*77,v=game.board[row][col];
        ui_paint_hitbox(x,y,73,75);ui_paint_ring(x+36,y+37,29,2);
        if(v)ui_paint_ellipse(x+36,y+37,25,25,v==1 ? 0 : 170);
    }
}
static int input(const Input *in) {
    if(in->kind!=INPUT_TAP)return 1;
    if(in->y>=74 && in->y<118) {
        if(in->x>=528){memset(&game,0,sizeof(game));game.turn=1;dirty=1;}
        else if(in->x>=468 && in->x<512)game_help(title(),
            "Tippe eine Spalte an. Der Stein fällt auf den untersten freien Platz. Vier eigene Steine in einer Reihe, Spalte oder Diagonale gewinnen.",
            "Tap a column to drop a stone into its lowest free square. Four of your stones in a row, column or diagonal win.",
            "Touchez une colonne pour y déposer un pion. Alignez quatre pions horizontalement, verticalement ou en diagonale.");
    } else if(!game.winner && in->x>=40 && in->x<565 && in->y>=210 && in->y<672) {
        int col=(in->x-40)/75,row=ROWS-1;
        while(row>=0 && game.board[row][col])row--;
        if(row<0)return 1;
        int player=game.turn;
        game.board[row][col]=player;game.moves++;dirty=1;
        static const int dx[]={1,0,1,1},dy[]={0,1,1,-1};
        for(int d=0;d<4;d++) {
            int n=1;
            for(int sign=-1;sign<=1;sign+=2)for(int step=1;step<4;step++) {
                int x=col+dx[d]*step*sign,y=row+dy[d]*step*sign;
                if(x<0 || x>=COLS || y<0 || y>=ROWS || game.board[y][x]!=player)break;
                n++;
            }
            if(n>=4)game.winner=player;
        }
        if(!game.winner && game.moves==ROWS*COLS)game.winner=3;
        game.turn=3-player;
    }
    return 1;
}
static void save(void){if(dirty && app_state_save("game.bin",&game,sizeof(game)))dirty=0;}
int main(void) {
    if(!app_state_load("game.bin",&game,sizeof(game))) {
        int old[152];
        if(lumen_legacy_state("game16-113.bin",old,sizeof(old)) &&
           old[144]>=1 && old[144]<=2 && old[145]>=0 && old[145]<=3 && old[146]>=0 && old[146]<=42) {
            int valid=1;
            for(int i=0;i<42;i++)if(old[i]<0 || old[i]>2)valid=0;
            if(valid) {
                for(int i=0;i<42;i++)game.board[i/COLS][i%COLS]=old[i];
                game.turn=old[144];game.winner=old[145];game.moves=old[146];dirty=1;
            }
        }
    }
    if(game.turn<1 || game.turn>2 || game.winner<0 || game.winner>3 || game.moves<0 || game.moves>42)
        {memset(&game,0,sizeof(game));game.turn=1;}
    lumen_run(draw,input,save);return 0;
}
