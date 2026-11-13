#include "../games/package_ui.h"

enum { LEFT=28, TOP=190, CELL_WIDTH=68, CELL_HEIGHT=64 };
static unsigned char tour[64]={1};
static int square,moves=1,dirty;

static const char *word(const char *de,const char *en,const char *fr) {
    return lumen_language==1 ? en : lumen_language==2 ? fr : de;
}
static int reachable(int from,int to) {
    return abs(from%8-to%8)*abs(from/8-to/8)==2;
}
static void load(void) {
    unsigned char saved[64];
    if(!app_state_load("tour.bin",saved,sizeof(saved)))return;
    int path[65],count=0;
    for(int i=0;i<65;i++)path[i]=-1;
    for(int i=0;i<64;i++) {
        int step=saved[i];
        if(!step)continue;
        if(step>64 || path[step]>=0)return;
        path[step]=i;count++;
    }
    if(!count || path[1]!=0)return;
    for(int i=2;i<=count;i++)
        if(path[i]<0 || !reachable(path[i-1],path[i]))return;
    memcpy(tour,saved,sizeof(tour));moves=count;square=path[count];
}
static void save(void) {
    if(dirty && app_state_save("tour.bin",tour,sizeof(tour)))dirty=0;
}
static void draw(void) {
    ui_paint_heading(word("Springertour","Knight’s Tour","Parcours du cavalier"),3);
    lumen_button_action("help",408,74,44,44,"?",0);
    lumen_icon_action("undo",468,74,IC_UNDO);
    lumen_icon_action("restart",528,74,IC_RESTART);
    int available=0;
    for(int i=0;i<64;i++)available+=!tour[i] && reachable(square,i);
    char label[96];
    snprintf(label,sizeof(label),"%d / 64%s",moves,moves==64 ?
        word(" · Geschafft!"," · Complete!"," · Terminé !") : available ? "" :
        word(" · Keine weiteren Züge"," · No moves left"," · Aucun coup restant"));
    ui_paint_text(28,132,544,45,label,PAINT_SECONDARY);
    for(int i=0;i<64;i++) {
        int col=i%8,row=i/8,x=LEFT+col*CELL_WIDTH,y=TOP+row*CELL_HEIGHT;
        int gray=(col+row)%2 ? 225 : 255;
        ui_paint_surface(x,y,CELL_WIDTH,CELL_HEIGHT,0,gray,gray);
        ui_paint_hitbox(x,y,CELL_WIDTH,CELL_HEIGHT);
        if(i==square) {
            lumen_cmd("chess_piece",x+10,y+7,48,48);
            fputs(",\"piece\":2}",stdout);
        } else if(tour[i]) {
            snprintf(label,sizeof(label),"%d",tour[i]);
            lumen_text(x,y,CELL_WIDTH,CELL_HEIGHT,label,PAINT_SECONDARY,95,0);
        } else if(reachable(square,i))ui_paint_ellipse(x+34,y+32,5,5,0);
    }
}
static int input(const Input *in) {
    if(in->kind==INPUT_COMPONENT) {
        char action[32];
        if(!game_action(in,action,sizeof(action)))return 0;
        if(!strcmp(action,"help"))
            lumen_app_open_with("lumen.reader","open_document","",
                word("Springertour","Knight’s Tour","Parcours du cavalier"),
                word("Besuche alle 64 Felder genau einmal. Der Springer zieht zwei Felder in eine Richtung und eines quer dazu. Punkte zeigen mögliche Züge.",
                     "Visit all 64 squares exactly once. A knight moves two squares in one direction and one across. Dots mark legal moves.",
                     "Visitez les 64 cases une seule fois. Le cavalier avance de deux cases dans une direction et d’une case de côté. Les points indiquent les coups autorisés."));
        else if(!strcmp(action,"undo") && moves>1) {
            tour[square]=0;moves--;dirty=1;
            for(int i=0;i<64;i++)if(tour[i]==moves){square=i;break;}
        } else if(!strcmp(action,"restart")) {
            memset(tour,0,sizeof(tour));square=0;tour[0]=1;moves=1;dirty=1;
        }
        return 1;
    }
    if(in->kind!=INPUT_TAP)return 0;
    if(in->x<LEFT || in->x>=LEFT+8*CELL_WIDTH || in->y<TOP || in->y>=TOP+8*CELL_HEIGHT)
        return 0;
    int to=(in->y-TOP)/CELL_HEIGHT*8+(in->x-LEFT)/CELL_WIDTH;
    if(tour[to] || !reachable(square,to))return 0;
    square=to;tour[to]=++moves;dirty=1;
    return 1;
}
int main(void) {
    load();
    lumen_run(draw,input,save);
    return 0;
}
