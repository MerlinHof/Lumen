#include "../../../tools/sdk/lumen_geometry.h"

/* Shared editable-text popup. The same geometry drives drawing and touch. */
enum { EDIT_SELECTION_HEIGHT=158 };
static void selection_menu_draw(int y,const char *text,const char *const actions[4]) {
    raised_surface(20,y,556,EDIT_SELECTION_HEIGHT);
    label_ellipsis(35,y+4,474,41,text,&small,0);
    icon_button(521,y+1,IC_CLOSE);
    for(int i=0;i<4;i++)
        button_h(35+i%2*272,y+46+i/2*52,252,44,actions[i],0);
}
static int selection_menu_action(int top,int x,int y) {
    y-=top;
    if(x>=521 && x<565 && y>=1 && y<45)return 4;
    for(int i=0;i<4;i++) {
        int left=35+i%2*272,row=46+i/2*52;
        if(x>=left && x<left+252 && y>=row && y<row+44)return i;
    }
    return -1;
}
