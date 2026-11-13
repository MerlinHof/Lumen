#include "../games/package_ui.h"

static signed char cells[25];
static int dirty;
static int won(void) {
    for(int i=0;i<5;i++) {
        int row=1,col=1;
        for(int j=0;j<5;j++){row&=cells[5*i+j]<0;col&=cells[5*j+i]<0;}
        if(row || col)return 1;
    }
    int a=1,b=1;
    for(int i=0;i<5;i++){a&=cells[6*i]<0;b&=cells[4*(i+1)]<0;}
    return a || b;
}
static void reset(void) {
    unsigned rng=(unsigned)time(NULL)^(unsigned)ui_clock_millis();
    for(int col=0;col<5;col++) {
        int numbers[15];
        for(int j=0;j<15;j++)numbers[j]=15*col+j+1;
        for(int row=0;row<5;row++) {
            unsigned i=row+game_random(&rng)%(15-row);
            cells[row*5+col]=numbers[i];numbers[i]=numbers[row];
        }
    }
    cells[12]=-1;dirty=1;
}
static void draw(void) {
    ui_paint_heading("Bingo",2);
    lumen_icon_action("help",468,74,IC_BOOK);lumen_icon_action("restart",528,74,IC_RESTART);
    for(int i=0;i<5;i++) {
        char letter[]={"BINGO"[i],0};
        ui_paint_text(35+i*106,140,100,44,letter,PAINT_HEADING);
    }
    for(int i=0;i<25;i++) {
        int x=35+i%5*106,y=200+i/5*100,v=cells[i];
        char text[10];
        if(i==12)snprintf(text,sizeof(text),"★");
        else snprintf(text,sizeof(text),"%d",abs(v));
        ui_paint_hitbox(x,y,100,94);ui_paint_surface(x,y,100,94,12,255,1);
        if(v<0)ui_paint_ellipse(x+50,y+47,35,35,0);
        lumen_text(x,y,100,94,text,PAINT_HEADING,v<0 ? 255 : 0,0);
    }
    if(won())ui_paint_text(28,718,544,45,"BINGO!",PAINT_DISPLAY);
}
static int input(const Input *in) {
    char action[32];
    if(in->kind!=INPUT_TAP && !game_action(in,action,sizeof(action)))return 0;
    if(in->kind==INPUT_COMPONENT) {
        if(!strcmp(action,"restart"))reset();
        else if(!strcmp(action,"help"))game_help("Bingo",
            "Tippe jede aufgerufene Zahl an. Das mittlere Feld ist frei. Fünf markierte Felder in einer Reihe, Spalte oder Diagonale ergeben Bingo.",
            "Tap each called number. The center is free. Five marked squares in a row, column or diagonal make bingo.",
            "Touchez chaque numéro annoncé. La case centrale est libre. Cinq cases marquées sur une ligne, une colonne ou une diagonale font bingo.");
    } else if(in->x>=35 && in->x<565 && in->y>=200 && in->y<700) {
        int i=(in->y-200)/100*5+(in->x-35)/106;
        if(i!=12){cells[i]=-cells[i];dirty=1;}
    }
    return 1;
}
static void save(void){if(dirty && app_state_save("game.bin",cells,sizeof(cells)))dirty=0;}
int main(void) {
    int valid=app_state_load("game.bin",cells,sizeof(cells));
    if(!valid) {
        int old[152];
        if(lumen_legacy_state("game16-112.bin",old,sizeof(old))) {
            valid=1;
            for(int i=0;i<25;i++) {
                if(!old[i] || old[i]<-75 || old[i]>75)valid=0;
                cells[i]=old[i];
            }
            dirty=valid;
        }
    }
    if(!valid)reset();
    lumen_run(draw,input,save);return 0;
}
