#ifndef LUMEN_PACKAGED_GAME_UI_H
#define LUMEN_PACKAGED_GAME_UI_H
#include "lumen_app.h"

static const char *game_text(const char *de,const char *en,const char *fr) {
    return lumen_language==1 ? en : lumen_language==2 ? fr : de;
}
static void game_help(const char *title,const char *de,const char *en,const char *fr) {
    lumen_request("reader",title,game_text(de,en,fr),0);
}
static unsigned game_random(unsigned *seed) {
    *seed=*seed*1664525u+1013904223u;
    return *seed>>8;
}
static inline int game_action(const Input *input,char *id,size_t capacity) {
    char gesture[24];
    return input->kind==INPUT_COMPONENT && wire_bounded_string("id",id,capacity) &&
        wire_bounded_string("gesture",gesture,sizeof(gesture)) && !strcmp(gesture,"clicked");
}

static const struct { const char *id; int icon,dx,dy; } game_directions[]={
    {"left",IC_PREV,-1,0},{"up",IC_UP,0,-1},{"down",IC_DOWN,0,1},{"right",IC_NEXT,1,0}
};

static inline void game_direction_pad(void) {
    for (int i=0;i<4;i++) {
        int x=150+i*79;
        lumen_button(x,728,60,44,"",0);
        ui_paint_icon_button(x+8,728,game_directions[i].icon);
        lumen_custom_action(game_directions[i].id,x,728,60,44);
    }
}

static inline void game_direction(const char *id,int *dx,int *dy) {
    for (int i=0;i<4;i++)
        if (!strcmp(id,game_directions[i].id)) {
            *dx=game_directions[i].dx;
            *dy=game_directions[i].dy;
            return;
        }
}
#endif
