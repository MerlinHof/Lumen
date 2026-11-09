#include "lumen_app.h"
#include "../games/puzzles.h"

static Sudoku game;
static int selected=-1,pencil,dirty;
static const char *word(const char *de,const char *en,const char *fr) {
    return lumen_language==1 ? en : lumen_language==2 ? fr : de;
}
static void draw(void) {
    ui_paint_heading("Sudoku",2);
    ui_paint_icon_button(468,74,IC_BOOK);
    ui_paint_icon_button(528,74,IC_RESTART);
    lumen_button(28,119,140,42,word("Notizen","Pencil","Crayon"),pencil);
    ui_paint_icon_button(184,118,IC_UNDO);
    ui_paint_icon_button(240,118,IC_DELETE);
    lumen_button(360,119,150,42,word("Hinweis","Hint","Indice"),0);
    char text[8];
    for(int i=0;i<81;i++) {
        int x=39+i%9*58,y=170+i/9*58;
        if(i==selected)ui_paint_surface(x+1,y+1,56,56,5,215,0);
        else if(selected>=0 && game.board.cell[selected] && game.board.cell[i]==game.board.cell[selected])
            ui_paint_surface(x+1,y+1,56,56,5,232,0);
        ui_paint_hitbox(x,y,58,58);
        if(game.board.cell[i]) {
            snprintf(text,sizeof(text),"%d",game.board.cell[i]);
            lumen_text(x,y,58,58,text,game.given[i] ? PAINT_HEADING : PAINT_BODY,game.given[i] ? 0 : 70,0);
            if(sudoku_conflict(&game,i))ui_paint_line(x+15,y+48,x+42,y+48,3,0);
        } else for(int k=0;k<9;k++)if(game.board.notes[i]&(1<<k)) {
            snprintf(text,sizeof(text),"%d",k+1);
            ui_paint_text(x+k%3*19,y+k/3*19,20,20,text,PAINT_SECONDARY);
        }
    }
    for(int i=0;i<=9;i++) {
        int weight=i%3 ? 1 : 3, gray=i%3 ? 182 : 50;
        ui_paint_line(39+i*58,170,39+i*58,692,weight,gray);
        ui_paint_line(39,170+i*58,561,170+i*58,weight,gray);
    }
    for(int v=1;v<=9;v++){snprintf(text,sizeof(text),"%d",v);lumen_button(28+(v-1)*61,725,56,52,text,0);}
    if(sudoku_won(&game))ui_paint_text(28,692,544,28,word("Geschafft!","Solved!","Résolu !"),PAINT_BODY);
}
static int input(const Input *in) {
    if(in->kind!=INPUT_TAP)return 1;
    int x=in->x,y=in->y;
    if(y>=74 && y<118) {
        if(x>=528){sudoku_new(&game,(uint32_t)time(NULL),game.difficulty);selected=-1;dirty=1;}
        else if(x>=468 && x<512)lumen_request("reader","Sudoku",word(
            "# Sudoku\n\nJede Reihe, Spalte und jeder 3×3-Block enthält die Ziffern 1 bis 9 einmal. Tippe ein Feld und danach eine Ziffer an. Im Notizmodus kannst du Kandidaten festhalten.",
            "# Sudoku\n\nEvery row, column and 3×3 box contains digits 1 to 9 exactly once. Tap a square, then a digit. Use Pencil to mark possible digits.",
            "# Sudoku\n\nChaque ligne, colonne et bloc 3×3 contient les chiffres de 1 à 9 une seule fois. Touchez une case, puis un chiffre. Le crayon note les chiffres possibles."),0);
    } else if(y>=118 && y<162) {
        if(x>=28 && x<168)pencil^=1;
        else if(x>=184 && x<228)dirty|=sudoku_undo(&game);
        else if(x>=240 && x<284)dirty|=sudoku_put(&game,selected,0,0);
        else if(x>=360 && x<510)dirty|=sudoku_hint(&game,selected);
    } else if(x>=39 && x<561 && y>=170 && y<692)selected=(y-170)/58*9+(x-39)/58;
    else if(y>=725 && y<777 && x>=28 && x<573 && (x-28)%61<56)
        dirty|=sudoku_put(&game,selected,(x-28)/61+1,pencil);
    return 1;
}
static void save(void){if(dirty && app_state_save("game.bin",&game,sizeof(game)))dirty=0;}
int main(void) {
    int valid=app_state_load("game.bin",&game,sizeof(game)) ||
              lumen_legacy_state("sudoku-v1.bin",&game,sizeof(game));
    for(int i=0;i<81;i++)
        if(game.board.cell[i]>9 || game.solution[i]<1 || game.solution[i]>9 || game.given[i]>9)valid=0;
    if(!valid || game.undos<0 || game.undos>24)sudoku_new(&game,(uint32_t)time(NULL),0);
    dirty=1;lumen_run(draw,input,save);return 0;
}
