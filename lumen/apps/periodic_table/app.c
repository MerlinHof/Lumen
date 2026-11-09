#include "../games/package_ui.h"
#include "data.h"

static int selected=6,element_grid16[9][18],dirty;
#include "model.h"
#include "labels.h"

static void draw(void) {
    ui_paint_heading(game_text("Periodensystem","Periodic Table","Tableau périodique"),0);
    const char **element=elements16[selected-1];
    for(int row=0;row<9;row++)for(int col=0;col<18;col++) {
        int number=element_grid16[row][col];
        if(!number)continue;
        int x=12+col*32,y=138+row*36;
        ui_paint_hitbox(x,y,31,34);
        ui_paint_surface(x,y,31,34,7,number==selected ? 0 : 244,0);
        const char *symbol=elements16[number-1][0];
        int width=(int)strlen(symbol)*12;
        lumen_mono(x+(31-width)/2,y,width,34,symbol,number==selected ? 255 : 0);
    }
    char label[130];
    snprintf(label,sizeof(label),"%d  %s · %s",selected,element[0],
             localized(selected==80 ? "Quecksilber" : element[1]));
    lumen_text(28,486,544,44,label,PAINT_HEADING,0,1);
    snprintf(label,sizeof(label),"%s · %s",localized(element[9]),localized(element[5]));
    lumen_text(28,531,544,30,label,PAINT_SECONDARY,96,1);
    const char *labels[]={"Atommasse","Dichte","Elektronegativität","Entdeckung","Schmelzpunkt","Siedepunkt"};
    const char *values[]={element[2],element[8],element[4],element[10],element[6],element[7]};
    const char *units[]={"u","g/cm³","","","K","K"};
    for(int i=0;i<6;i++) {
        int x=28+i%2*282,y=575+i/2*57;
        lumen_text(x,y,262,24,localized(labels[i]),PAINT_SECONDARY,96,1);
        snprintf(label,sizeof(label),"%s%s%s",*values[i] ? localized(values[i]) : "—",
                 *values[i] && *units[i] ? " " : "",*values[i] ? units[i] : "");
        lumen_text(x,y+24,262,28,label,PAINT_BODY,0,1);
    }
    ui_paint_line(28,751,572,751,1,185);
    lumen_text(28,760,544,30,element[3],PAINT_SECONDARY,96,1);
}
static int input(const Input *in) {
    if(in->kind!=INPUT_TAP || in->x<12 || in->x>=588 || in->y<138 || in->y>=462)return 1;
    int number=element_grid16[(in->y-138)/36][(in->x-12)/32];
    if(number && number!=selected){selected=number;dirty=1;}
    return 1;
}
static void save(void){if(dirty && app_state_save("selected.bin",&selected,sizeof(selected)))dirty=0;}
int main(void) {
    app_state_load("selected.bin",&selected,sizeof(selected));
    if(selected<1 || selected>118)selected=6;
    elements_init16();lumen_run(draw,input,save);return 0;
}
