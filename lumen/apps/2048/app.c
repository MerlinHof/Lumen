#include "lumen_app.h"
#include "../games/puzzles.h"

static Tiles game;
static int dirty;
static const char *word(const char *de,const char *en,const char *fr) {
    return lumen_language==1 ? en : lumen_language==2 ? fr : de;
}
static void draw(void) {
    ui_paint_heading("2048",3);
    ui_paint_icon_button(408,74,IC_BOOK);
    ui_paint_icon_button(468,74,IC_UNDO);
    ui_paint_icon_button(528,74,IC_RESTART);
    char text[96];
    snprintf(text,sizeof(text),"%s  %llu",word("Punkte","Score","Score"),(unsigned long long)game.state.score);
    ui_paint_text(28,132,544,40,text,PAINT_HEADING);
    const int grays[]={247,232,219,192,176,159,144,124,105,87,68,48};
    for(int i=0;i<16;i++) {
        int x=56+i%4*124,y=184+i/4*124,v=game.state.cell[i];
        int gray=grays[v<12 ? v : 11];
        ui_paint_surface(x,y,116,116,12,gray,0);
        if(v) {
            snprintf(text,sizeof(text),"%u",1u<<v);
            lumen_text(x,y,116,116,text,v>=10 ? PAINT_HEADING : PAINT_DISPLAY,gray<140 ? 255 : 0,0);
        }
    }
    const int arrows[]={IC_PREV,IC_UP,IC_DOWN,IC_NEXT};
    for(int i=0;i<4;i++)ui_paint_icon_button(66+i*140,727,arrows[i]);
    if(tiles_over(&game))ui_paint_text(28,684,544,32,word("Keine Züge mehr","No moves left","Plus de coups"),PAINT_SECONDARY);
    else if(game.state.won)ui_paint_text(28,684,544,32,word("2048 erreicht!","2048 reached!","2048 atteint !"),PAINT_SECONDARY);
}
static int input(const Input *in) {
    if(in->kind==INPUT_SWIPE) {
        int dir=abs(in->dx)>abs(in->dy) ? (in->dx<0 ? 0 : 1) : (in->dy<0 ? 2 : 3);
        dirty|=tiles_move(&game,dir);
    }
    if(in->kind!=INPUT_TAP)return 1;
    if(in->y>=74 && in->y<118) {
        if(in->x>=528) {tiles_new(&game,(uint32_t)time(NULL));dirty=1;}
        else if(in->x>=468 && game.can_undo) {game.state=game.undo;game.can_undo=0;dirty=1;}
        else if(in->x>=408 && in->x<452)lumen_request("reader","2048",word(
            "# 2048\n\nWische oder nutze die Pfeile. Gleiche Steine verschmelzen einmal pro Zug. Erreiche 2048 und spiele für einen höheren Rekord weiter. Rückgängig stellt den vorigen Zug wieder her.",
            "# 2048\n\nSwipe or use the arrows. Matching tiles merge once per move. Reach 2048, then keep playing for a higher score. Undo restores the previous move.",
            "# 2048\n\nGlissez ou utilisez les flèches. Les tuiles identiques fusionnent une fois par coup. Atteignez 2048, puis améliorez votre score. Annuler restaure le coup précédent."),0);
    }
    if(in->y>=727 && in->y<771 && in->x>=66) {
        int i=(in->x-66)/140,dir[]={0,2,3,1};
        if(i<4 && (in->x-66)%140<44)dirty|=tiles_move(&game,dir[i]);
    }
    return 1;
}
static void save(void) {if(dirty && app_state_save("game.bin",&game,sizeof(game)))dirty=0;}
int main(void) {
    int valid=app_state_load("game.bin",&game,sizeof(game)) ||
              lumen_legacy_state("2048-v1.bin",&game,sizeof(game));
    for(int i=0;i<16;i++)if(game.state.cell[i]>30 || game.undo.cell[i]>30)valid=0;
    if(!valid)tiles_new(&game,(uint32_t)time(NULL));
    dirty=1;
    lumen_run(draw,input,save);
    return 0;
}
